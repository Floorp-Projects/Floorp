/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"
#include "DocAccessibleWrap.h"
#include "SessionAccessibility.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessiblePivot.h"
#include "nsIStringBundle.h"

#define ROLE_STRINGS_URL "chrome://global/locale/AccessFu.properties"

using namespace mozilla;
using namespace mozilla::a11y;

static nsIStringBundle* sStringBundle;

void a11y::PlatformInit() {}

void a11y::PlatformShutdown() { NS_IF_RELEASE(sStringBundle); }

void a11y::ProxyCreated(RemoteAccessible* aProxy) {
  SessionAccessibility::RegisterAccessible(aProxy);
}

void a11y::ProxyDestroyed(RemoteAccessible* aProxy) {
  SessionAccessibility::UnregisterAccessible(aProxy);
}

void a11y::ProxyEvent(RemoteAccessible* aTarget, uint32_t aEventType) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);
  if (!sessionAcc) {
    return;
  }

  switch (aEventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      sessionAcc->SendFocusEvent(aTarget);
      break;
    case nsIAccessibleEvent::EVENT_REORDER:
      if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
        sessionAcc->SendWindowContentChangedEvent();
      }
      break;
    default:
      break;
  }
}

void a11y::ProxyStateChangeEvent(RemoteAccessible* aTarget, uint64_t aState,
                                 bool aEnabled) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (!sessionAcc) {
    return;
  }

  if (aState & states::CHECKED) {
    sessionAcc->SendClickedEvent(
        aTarget, java::SessionAccessibility::FLAG_CHECKABLE |
                     (aEnabled ? java::SessionAccessibility::FLAG_CHECKED : 0));
  }

  if (aState & states::EXPANDED) {
    sessionAcc->SendClickedEvent(
        aTarget,
        java::SessionAccessibility::FLAG_EXPANDABLE |
            (aEnabled ? java::SessionAccessibility::FLAG_EXPANDED : 0));
  }

  if (aState & states::SELECTED) {
    sessionAcc->SendSelectedEvent(aTarget, aEnabled);
  }

  if (aState & states::BUSY) {
    sessionAcc->SendWindowStateChangedEvent(aTarget);
    if (StaticPrefs::accessibility_cache_enabled_AtStartup()) {
      sessionAcc->SendWindowContentChangedEvent();
    }
  }
}

void a11y::ProxyCaretMoveEvent(RemoteAccessible* aTarget, int32_t aOffset,
                               bool aIsSelectionCollapsed,
                               int32_t aGranularity) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendTextSelectionChangedEvent(aTarget, aOffset);
  }
}

void a11y::ProxyTextChangeEvent(RemoteAccessible* aTarget, const nsString& aStr,
                                int32_t aStart, uint32_t aLen, bool aIsInsert,
                                bool aFromUser) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendTextChangedEvent(aTarget, aStr, aStart, aLen, aIsInsert,
                                     aFromUser);
  }
}

void a11y::ProxyShowHideEvent(RemoteAccessible* aTarget,
                              RemoteAccessible* aParent, bool aInsert,
                              bool aFromUser) {
  // We rely on the window content changed events to be dispatched
  // after the viewport cache is refreshed.
}

void a11y::ProxySelectionEvent(RemoteAccessible*, RemoteAccessible*, uint32_t) {
}

void a11y::ProxyVirtualCursorChangeEvent(
    RemoteAccessible* aTarget, RemoteAccessible* aOldPosition,
    int32_t aOldStartOffset, int32_t aOldEndOffset,
    RemoteAccessible* aNewPosition, int32_t aNewStartOffset,
    int32_t aNewEndOffset, int16_t aReason, int16_t aBoundaryType,
    bool aFromUser) {
  if (!aNewPosition || !aFromUser) {
    return;
  }

  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (!sessionAcc) {
    return;
  }

  if (aReason == nsIAccessiblePivot::REASON_POINT) {
    sessionAcc->SendHoverEnterEvent(aNewPosition);
  } else if (aBoundaryType == nsIAccessiblePivot::NO_BOUNDARY) {
    sessionAcc->SendAccessibilityFocusedEvent(aNewPosition);
  }

  if (aBoundaryType != nsIAccessiblePivot::NO_BOUNDARY) {
    sessionAcc->SendTextTraversedEvent(aNewPosition, aNewStartOffset,
                                       aNewEndOffset);
  }
}

void a11y::ProxyScrollingEvent(RemoteAccessible* aTarget, uint32_t aEventType,
                               uint32_t aScrollX, uint32_t aScrollY,
                               uint32_t aMaxScrollX, uint32_t aMaxScrollY) {
  if (aEventType == nsIAccessibleEvent::EVENT_SCROLLING) {
    RefPtr<SessionAccessibility> sessionAcc =
        SessionAccessibility::GetInstanceFor(aTarget);

    if (sessionAcc) {
      sessionAcc->SendScrollingEvent(aTarget, aScrollX, aScrollY, aMaxScrollX,
                                     aMaxScrollY);
    }
  }
}

void a11y::ProxyAnnouncementEvent(RemoteAccessible* aTarget,
                                  const nsString& aAnnouncement,
                                  uint16_t aPriority) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendAnnouncementEvent(aTarget, aAnnouncement, aPriority);
  }
}

void a11y::ProxyBatch(RemoteAccessible* aDocument, const uint64_t aBatchType,
                      const nsTArray<RemoteAccessible*>& aAccessibles,
                      const nsTArray<BatchData>& aData) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aDocument);
  if (!sessionAcc) {
    return;
  }

  nsTArray<Accessible*> accessibles(aAccessibles.Length());
  for (size_t i = 0; i < aAccessibles.Length(); i++) {
    accessibles.AppendElement(aAccessibles.ElementAt(i));
  }

  switch (aBatchType) {
    case DocAccessibleWrap::eBatch_Viewport:
      sessionAcc->ReplaceViewportCache(accessibles, aData);
      break;
    case DocAccessibleWrap::eBatch_FocusPath:
      sessionAcc->ReplaceFocusPathCache(accessibles, aData);
      break;
    case DocAccessibleWrap::eBatch_BoundsUpdate:
      sessionAcc->UpdateCachedBounds(accessibles, aData);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unknown batch type.");
      break;
  }
}

bool a11y::LocalizeString(const char* aToken, nsAString& aLocalized,
                          const nsTArray<nsString>& aFormatString) {
  MOZ_ASSERT(XRE_IsParentProcess());
  nsresult rv = NS_OK;
  if (!sStringBundle) {
    nsCOMPtr<nsIStringBundleService> sbs = components::StringBundle::Service();
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get string bundle service");
      return false;
    }

    nsCOMPtr<nsIStringBundle> sb;
    rv = sbs->CreateBundle(ROLE_STRINGS_URL, getter_AddRefs(sb));
    if (NS_FAILED(rv)) {
      NS_WARNING("Failed to get string bundle");
      return false;
    }

    sb.forget(&sStringBundle);
  }

  MOZ_ASSERT(sStringBundle);

  if (aFormatString.Length()) {
    rv = sStringBundle->FormatStringFromName(aToken, aFormatString, aLocalized);
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  } else {
    rv = sStringBundle->GetStringFromName(aToken, aLocalized);
    if (NS_SUCCEEDED(rv)) {
      return true;
    }
  }

  NS_WARNING("Failed to localize string");
  aLocalized.AssignLiteral("");
  return false;
}

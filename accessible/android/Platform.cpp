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
#include "nsIAccessibleEvent.h"
#include "nsIAccessiblePivot.h"
#include "nsIStringBundle.h"
#include "TextLeafRange.h"

#define ROLE_STRINGS_URL "chrome://global/locale/AccessFu.properties"

using namespace mozilla;
using namespace mozilla::a11y;

static nsTHashMap<nsStringHashKey, nsString> sLocalizedStrings;

void a11y::PlatformInit() {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIStringBundleService> stringBundleService =
      components::StringBundle::Service();
  if (!stringBundleService) return;

  nsCOMPtr<nsIStringBundle> stringBundle;
  nsCOMPtr<nsIStringBundleService> sbs = components::StringBundle::Service();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get string bundle service");
    return;
  }

  rv = sbs->CreateBundle(ROLE_STRINGS_URL, getter_AddRefs(stringBundle));
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to get string bundle");
    return;
  }

  nsString localizedStr;
  // Preload the state required localized string.
  rv = stringBundle->GetStringFromName("stateRequired", localizedStr);
  if (NS_SUCCEEDED(rv)) {
    sLocalizedStrings.InsertOrUpdate(u"stateRequired"_ns, localizedStr);
  }

  // Preload heading level localized descriptions 1 thru 6.
  for (int32_t level = 1; level <= 6; level++) {
    nsAutoString token;
    token.AppendPrintf("heading-%d", level);

    nsAutoString formatString;
    formatString.AppendInt(level);
    AutoTArray<nsString, 1> formatParams;
    formatParams.AppendElement(formatString);
    rv = stringBundle->FormatStringFromName("headingLevel", formatParams,
                                            localizedStr);
    if (NS_SUCCEEDED(rv)) {
      sLocalizedStrings.InsertOrUpdate(token, localizedStr);
    }
  }

  // Preload any roles that have localized versions
#define ROLE(geckoRole, stringRole, ariaRole, atkRole, macRole, macSubrole, \
             msaaRole, ia2Role, androidClass, nameRule)                     \
  rv = stringBundle->GetStringFromName(stringRole, localizedStr);           \
  if (NS_SUCCEEDED(rv)) {                                                   \
    sLocalizedStrings.InsertOrUpdate(u##stringRole##_ns, localizedStr);     \
  }

#include "RoleMap.h"
#undef ROLE
}

void a11y::PlatformShutdown() { sLocalizedStrings.Clear(); }

void a11y::ProxyCreated(RemoteAccessible* aProxy) {
  SessionAccessibility::RegisterAccessible(aProxy);
}

void a11y::ProxyDestroyed(RemoteAccessible* aProxy) {
  SessionAccessibility::UnregisterAccessible(aProxy);
}

void a11y::PlatformEvent(Accessible* aTarget, uint32_t aEventType) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);
  if (!sessionAcc) {
    return;
  }

  switch (aEventType) {
    case nsIAccessibleEvent::EVENT_REORDER:
      sessionAcc->SendWindowContentChangedEvent();
      break;
    case nsIAccessibleEvent::EVENT_SCROLLING_START:
      if (Accessible* result = AccessibleWrap::DoPivot(
              aTarget, java::SessionAccessibility::HTML_GRANULARITY_DEFAULT,
              true, true)) {
        sessionAcc->SendAccessibilityFocusedEvent(result);
      }
      break;
    default:
      break;
  }
}

void a11y::PlatformStateChangeEvent(Accessible* aTarget, uint64_t aState,
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
  }
}

void a11y::PlatformFocusEvent(Accessible* aTarget,
                              const LayoutDeviceIntRect& aCaretRect) {
  if (RefPtr<SessionAccessibility> sessionAcc =
          SessionAccessibility::GetInstanceFor(aTarget)) {
    sessionAcc->SendFocusEvent(aTarget);
  }
}

void a11y::PlatformCaretMoveEvent(Accessible* aTarget, int32_t aOffset,
                                  bool aIsSelectionCollapsed,
                                  int32_t aGranularity,
                                  const LayoutDeviceIntRect& aCaretRect,
                                  bool aFromUser) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);
  if (!sessionAcc) {
    return;
  }

  if (!aTarget->IsDoc() && !aFromUser && !aIsSelectionCollapsed) {
    // Pivot to the caret's position if it has an expanded selection.
    // This is used mostly for find in page.
    Accessible* leaf = TextLeafPoint::GetCaret(aTarget).ActualizeCaret().mAcc;
    MOZ_ASSERT(leaf);
    if (leaf) {
      if (Accessible* result = AccessibleWrap::DoPivot(
              leaf, java::SessionAccessibility::HTML_GRANULARITY_DEFAULT, true,
              true)) {
        sessionAcc->SendAccessibilityFocusedEvent(result);
      }
    }
  }

  sessionAcc->SendTextSelectionChangedEvent(aTarget, aOffset);
}

void a11y::PlatformTextChangeEvent(Accessible* aTarget, const nsAString& aStr,
                                   int32_t aStart, uint32_t aLen,
                                   bool aIsInsert, bool aFromUser) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendTextChangedEvent(aTarget, aStr, aStart, aLen, aIsInsert,
                                     aFromUser);
  }
}

void a11y::PlatformShowHideEvent(Accessible* aTarget, Accessible* aParent,
                                 bool aInsert, bool aFromUser) {
  // We rely on the window content changed events to be dispatched
  // after the viewport cache is refreshed.
}

void a11y::PlatformSelectionEvent(Accessible*, Accessible*, uint32_t) {}

void a11y::PlatformScrollingEvent(Accessible* aTarget, uint32_t aEventType,
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

void a11y::PlatformAnnouncementEvent(Accessible* aTarget,
                                     const nsAString& aAnnouncement,
                                     uint16_t aPriority) {
  RefPtr<SessionAccessibility> sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendAnnouncementEvent(aTarget, aAnnouncement, aPriority);
  }
}

bool a11y::LocalizeString(const nsAString& aToken, nsAString& aLocalized) {
  MOZ_ASSERT(XRE_IsParentProcess());

  auto str = sLocalizedStrings.Lookup(aToken);
  if (str) {
    aLocalized.Assign(*str);
  } else {
  }

  return !!str;
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Platform.h"
#include "ProxyAccessibleWrap.h"
#include "SessionAccessibility.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "nsIAccessibleEvent.h"
#include "nsIAccessiblePivot.h"

using namespace mozilla;
using namespace mozilla::a11y;

void
a11y::PlatformInit()
{
}

void
a11y::PlatformShutdown()
{
}

void
a11y::ProxyCreated(ProxyAccessible* aProxy, uint32_t aInterfaces)
{
  AccessibleWrap* wrapper = nullptr;
  if (aProxy->IsDoc()) {
    wrapper = new DocProxyAccessibleWrap(aProxy->AsDoc());
  } else {
    wrapper = new ProxyAccessibleWrap(aProxy);
  }

  wrapper->AddRef();
  aProxy->SetWrapper(reinterpret_cast<uintptr_t>(wrapper));
}

void
a11y::ProxyDestroyed(ProxyAccessible* aProxy)
{
  AccessibleWrap* wrapper =
    reinterpret_cast<AccessibleWrap*>(aProxy->GetWrapper());

  // If aProxy is a document that was created, but
  // RecvPDocAccessibleConstructor failed then aProxy->GetWrapper() will be
  // null.
  if (!wrapper) {
    return;
  }

  wrapper->Shutdown();
  aProxy->SetWrapper(0);
  wrapper->Release();
}

void
a11y::ProxyEvent(ProxyAccessible* aTarget, uint32_t aEventType)
{
  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);
  if (!sessionAcc) {
    return;
  }

  switch (aEventType) {
    case nsIAccessibleEvent::EVENT_FOCUS:
      sessionAcc->SendFocusEvent(WrapperFor(aTarget));
      break;
  }
}

void
a11y::ProxyStateChangeEvent(ProxyAccessible* aTarget,
                            uint64_t aState,
                            bool aEnabled)
{
  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);

  if (!sessionAcc) {
    return;
  }

  if (aState & states::CHECKED) {
    sessionAcc->SendClickedEvent(WrapperFor(aTarget), aEnabled);
  }

  if (aState & states::SELECTED) {
    sessionAcc->SendSelectedEvent(WrapperFor(aTarget));
  }

  if (aState & states::BUSY) {
    sessionAcc->SendWindowStateChangedEvent(WrapperFor(aTarget));
  }
}

void
a11y::ProxyCaretMoveEvent(ProxyAccessible* aTarget, int32_t aOffset)
{
  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendTextSelectionChangedEvent(WrapperFor(aTarget), aOffset);
  }
}

void
a11y::ProxyTextChangeEvent(ProxyAccessible* aTarget,
                           const nsString& aStr,
                           int32_t aStart,
                           uint32_t aLen,
                           bool aIsInsert,
                           bool aFromUser)
{
  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);

  if (sessionAcc) {
    sessionAcc->SendTextChangedEvent(
      WrapperFor(aTarget), aStr, aStart, aLen, aIsInsert, aFromUser);
  }
}

void
a11y::ProxyShowHideEvent(ProxyAccessible* aTarget,
                         ProxyAccessible* aParent,
                         bool aInsert,
                         bool aFromUser)
{
  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);
  if (sessionAcc) {
    sessionAcc->SendWindowContentChangedEvent(WrapperFor(aParent));
  }
}

void
a11y::ProxySelectionEvent(ProxyAccessible*, ProxyAccessible*, uint32_t)
{
}

void
a11y::ProxyVirtualCursorChangeEvent(ProxyAccessible* aTarget,
                                    ProxyAccessible* aOldPosition,
                                    int32_t aOldStartOffset,
                                    int32_t aOldEndOffset,
                                    ProxyAccessible* aNewPosition,
                                    int32_t aNewStartOffset,
                                    int32_t aNewEndOffset,
                                    int16_t aReason,
                                    int16_t aBoundaryType,
                                    bool aFromUser)
{
  if (!aNewPosition) {
    return;
  }

  SessionAccessibility* sessionAcc =
    SessionAccessibility::GetInstanceFor(aTarget);

  if (!sessionAcc) {
    return;
  }

  if (aOldPosition != aNewPosition) {
    if (aReason == nsIAccessiblePivot::REASON_POINT) {
      sessionAcc->SendHoverEnterEvent(WrapperFor(aNewPosition));
    } else {
      sessionAcc->SendAccessibilityFocusedEvent(WrapperFor(aNewPosition));
    }
  }

  if (aBoundaryType != nsIAccessiblePivot::NO_BOUNDARY) {
    sessionAcc->SendTextTraversedEvent(
      WrapperFor(aNewPosition), aNewStartOffset, aNewEndOffset);
  }
}

void
a11y::ProxyScrollingEvent(ProxyAccessible* aTarget,
                          uint32_t aEventType,
                          uint32_t aScrollX,
                          uint32_t aScrollY,
                          uint32_t aMaxScrollX,
                          uint32_t aMaxScrollY)
{
  if (aEventType == nsIAccessibleEvent::EVENT_SCROLLING) {
    SessionAccessibility* sessionAcc =
      SessionAccessibility::GetInstanceFor(aTarget);

    if (sessionAcc) {
      sessionAcc->SendScrollingEvent(
        WrapperFor(aTarget), aScrollX, aScrollY, aMaxScrollX, aMaxScrollY);
    }
  }
}

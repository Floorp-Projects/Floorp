/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "Accessible-inl.h"
#include "AccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsMai.h"
#include "mozilla/Likely.h"
#include "mozilla/a11y/ProxyAccessible.h"

using namespace mozilla::a11y;

extern "C" {

static AtkObject* refAccessibleAtPointCB(AtkComponent* aComponent, gint aAccX,
                                         gint aAccY, AtkCoordType aCoordType) {
  return refAccessibleAtPointHelper(ATK_OBJECT(aComponent), aAccX, aAccY,
                                    aCoordType);
}

static void getExtentsCB(AtkComponent* aComponent, gint* aX, gint* aY,
                         gint* aWidth, gint* aHeight, AtkCoordType aCoordType) {
  getExtentsHelper(ATK_OBJECT(aComponent), aX, aY, aWidth, aHeight, aCoordType);
}

static gboolean grabFocusCB(AtkComponent* aComponent) {
  AtkObject* atkObject = ATK_OBJECT(aComponent);
  AccessibleWrap* accWrap = GetAccessibleWrap(atkObject);
  if (accWrap) {
    accWrap->TakeFocus();
    return TRUE;
  }

  ProxyAccessible* proxy = GetProxy(atkObject);
  if (proxy) {
    proxy->TakeFocus();
    return TRUE;
  }

  return FALSE;
}

// ScrollType is compatible
MOZ_CAN_RUN_SCRIPT_BOUNDARY
static gboolean scrollToCB(AtkComponent* aComponent, AtkScrollType type) {
  AtkObject* atkObject = ATK_OBJECT(aComponent);
  if (RefPtr<AccessibleWrap> accWrap = GetAccessibleWrap(atkObject)) {
    accWrap->ScrollTo(type);
    return TRUE;
  }

  ProxyAccessible* proxy = GetProxy(atkObject);
  if (proxy) {
    proxy->ScrollTo(type);
    return TRUE;
  }

  return FALSE;
}

// CoordType is compatible
static gboolean scrollToPointCB(AtkComponent* aComponent, AtkCoordType coords,
                                gint x, gint y) {
  AtkObject* atkObject = ATK_OBJECT(aComponent);
  AccessibleWrap* accWrap = GetAccessibleWrap(atkObject);
  if (accWrap) {
    accWrap->ScrollToPoint(coords, x, y);
    return TRUE;
  }

  ProxyAccessible* proxy = GetProxy(atkObject);
  if (proxy) {
    proxy->ScrollToPoint(coords, x, y);
    return TRUE;
  }

  return FALSE;
}
}

AtkObject* refAccessibleAtPointHelper(AtkObject* aAtkObj, gint aX, gint aY,
                                      AtkCoordType aCoordType) {
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  if (accWrap) {
    if (accWrap->IsDefunct() || nsAccUtils::MustPrune(accWrap)) {
      return nullptr;
    }

    // Accessible::ChildAtPoint(x,y) is in screen pixels.
    if (aCoordType == ATK_XY_WINDOW) {
      nsIntPoint winCoords =
          nsCoreUtils::GetScreenCoordsForWindow(accWrap->GetNode());
      aX += winCoords.x;
      aY += winCoords.y;
    }

    Accessible* accAtPoint =
        accWrap->ChildAtPoint(aX, aY, Accessible::eDirectChild);
    if (!accAtPoint) {
      return nullptr;
    }

    AtkObject* atkObj = AccessibleWrap::GetAtkObject(accAtPoint);
    if (atkObj) {
      g_object_ref(atkObj);
    }

    return atkObj;
  }

  if (ProxyAccessible* proxy = GetProxy(aAtkObj)) {
    ProxyAccessible* result =
        proxy->AccessibleAtPoint(aX, aY, aCoordType == ATK_XY_WINDOW);
    AtkObject* atkObj = result ? GetWrapperFor(result) : nullptr;
    if (atkObj) {
      g_object_ref(atkObj);
    }
    return atkObj;
  }

  return nullptr;
}

void getExtentsHelper(AtkObject* aAtkObj, gint* aX, gint* aY, gint* aWidth,
                      gint* aHeight, AtkCoordType aCoordType) {
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  *aX = *aY = *aWidth = *aHeight = 0;

  if (accWrap) {
    if (accWrap->IsDefunct()) {
      return;
    }

    nsIntRect screenRect = accWrap->Bounds();
    if (screenRect.IsEmpty()) return;

    if (aCoordType == ATK_XY_WINDOW) {
      nsIntPoint winCoords =
          nsCoreUtils::GetScreenCoordsForWindow(accWrap->GetNode());
      screenRect.x -= winCoords.x;
      screenRect.y -= winCoords.y;
    }

    *aX = screenRect.x;
    *aY = screenRect.y;
    *aWidth = screenRect.width;
    *aHeight = screenRect.height;
    return;
  }

  if (ProxyAccessible* proxy = GetProxy(aAtkObj)) {
    proxy->Extents(aCoordType == ATK_XY_WINDOW, aX, aY, aWidth, aHeight);
  }
}

void componentInterfaceInitCB(AtkComponentIface* aIface) {
  NS_ASSERTION(aIface, "Invalid Interface");
  if (MOZ_UNLIKELY(!aIface)) return;

  /*
   * Use default implementation in atk for contains, get_position,
   * and get_size
   */
  aIface->ref_accessible_at_point = refAccessibleAtPointCB;
  aIface->get_extents = getExtentsCB;
  aIface->grab_focus = grabFocusCB;
  if (IsAtkVersionAtLeast(2, 30)) {
    aIface->scroll_to = scrollToCB;
    aIface->scroll_to_point = scrollToPointCB;
  }
}

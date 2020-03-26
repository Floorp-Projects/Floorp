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
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/ProxyAccessible.h"
#include "mozilla/dom/BrowserParent.h"

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
  AccessibleOrProxy acc = GetInternalObj(aAtkObj);
  if (acc.IsNull()) {
    // This might be an ATK Socket.
    acc = GetAccessibleWrap(aAtkObj);
    if (acc.IsNull()) {
      return nullptr;
    }
  }
  if (acc.IsAccessible() && acc.AsAccessible()->IsDefunct()) {
    return nullptr;
  }

  // AccessibleOrProxy::ChildAtPoint(x,y) is in screen pixels.
  if (aCoordType == ATK_XY_WINDOW) {
    nsINode* node = nullptr;
    if (acc.IsAccessible()) {
      node = acc.AsAccessible()->GetNode();
    } else {
      // Use the XUL browser embedding this remote document.
      auto browser = static_cast<mozilla::dom::BrowserParent*>(
          acc.AsProxy()->Document()->Manager());
      node = browser->GetOwnerElement();
    }
    MOZ_ASSERT(node);
    nsIntPoint winCoords = nsCoreUtils::GetScreenCoordsForWindow(node);
    aX += winCoords.x;
    aY += winCoords.y;
  }

  AccessibleOrProxy accAtPoint =
      acc.ChildAtPoint(aX, aY, Accessible::eDeepestChild);
  if (accAtPoint.IsNull()) {
    return nullptr;
  }
  roles::Role role = accAtPoint.Role();
  if (role == roles::TEXT_LEAF || role == roles::STATICTEXT) {
    // We don't include text leaf nodes in the ATK tree, so return the parent.
    accAtPoint = accAtPoint.Parent();
    MOZ_ASSERT(!accAtPoint.IsNull(), "Text leaf should always have a parent");
  }
  AtkObject* atkObj = GetWrapperFor(accAtPoint);
  if (atkObj) {
    g_object_ref(atkObj);
  }
  return atkObj;
}

void getExtentsHelper(AtkObject* aAtkObj, gint* aX, gint* aY, gint* aWidth,
                      gint* aHeight, AtkCoordType aCoordType) {
  AccessibleWrap* accWrap = GetAccessibleWrap(aAtkObj);
  *aX = *aY = *aWidth = *aHeight = -1;

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

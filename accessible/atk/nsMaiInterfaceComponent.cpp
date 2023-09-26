/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "LocalAccessible-inl.h"
#include "AccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsMai.h"
#include "nsWindow.h"
#include "mozilla/Likely.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "mozilla/a11y/RemoteAccessible.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/Document.h"
#include "nsAccessibilityService.h"

using namespace mozilla;
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
  Accessible* acc = GetInternalObj(atkObject);
  if (acc) {
    acc->TakeFocus();
    return TRUE;
  }
  return FALSE;
}

// ScrollType is compatible
MOZ_CAN_RUN_SCRIPT_BOUNDARY
static gboolean scrollToCB(AtkComponent* aComponent, AtkScrollType type) {
  AtkObject* atkObject = ATK_OBJECT(aComponent);
  if (Accessible* acc = GetInternalObj(atkObject)) {
    acc->ScrollTo(type);
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

  RemoteAccessible* proxy = GetProxy(atkObject);
  if (proxy) {
    proxy->ScrollToPoint(coords, x, y);
    return TRUE;
  }

  return FALSE;
}
}

AtkObject* refAccessibleAtPointHelper(AtkObject* aAtkObj, gint aX, gint aY,
                                      AtkCoordType aCoordType) {
  Accessible* acc = GetInternalObj(aAtkObj);
  if (!acc) {
    return nullptr;
  }

  // Accessible::ChildAtPoint(x,y) is in screen pixels.
  if (aCoordType == ATK_XY_WINDOW) {
    mozilla::LayoutDeviceIntPoint winCoords =
        nsAccUtils::GetScreenCoordsForWindow(acc);
    aX += winCoords.x;
    aY += winCoords.y;
  }

  Accessible* accAtPoint =
      acc->ChildAtPoint(aX, aY, Accessible::EWhichChildAtPoint::DeepestChild);
  if (!accAtPoint) {
    return nullptr;
  }
  roles::Role role = accAtPoint->Role();
  if (role == roles::TEXT_LEAF || role == roles::STATICTEXT) {
    // We don't include text leaf nodes in the ATK tree, so return the parent.
    accAtPoint = accAtPoint->Parent();
    MOZ_ASSERT(accAtPoint, "Text leaf should always have a parent");
  }
  AtkObject* atkObj = GetWrapperFor(accAtPoint);
  if (atkObj) {
    g_object_ref(atkObj);
  }
  return atkObj;
}

static double getScaleFactor(Accessible* aAccessible) {
  DocAccessible* docAcc = nullptr;
  if (LocalAccessible* localAcc = aAccessible->AsLocal()) {
    docAcc = localAcc->Document();
  } else {
    RemoteAccessible* remote = aAccessible->AsRemote();
    LocalAccessible* outerDoc = remote->OuterDocOfRemoteBrowser();
    if (outerDoc) {
      docAcc = outerDoc->Document();
    }
  }

  if (!docAcc || !docAcc->DocumentNode()) {
    return 1.0;
  }

  nsCOMPtr<nsIWidget> rootWidget =
      nsContentUtils::WidgetForDocument(docAcc->DocumentNode());
  if (!rootWidget) {
    return 1.0;
  }

  if (RefPtr<nsWindow> window =
          static_cast<nsWindow*>(rootWidget->GetTopLevelWidget())) {
    return window->FractionalScaleFactor();
  }

  return 1.0;
}

void getExtentsHelper(AtkObject* aAtkObj, gint* aX, gint* aY, gint* aWidth,
                      gint* aHeight, AtkCoordType aCoordType) {
  *aX = *aY = *aWidth = *aHeight = -1;

  Accessible* acc = GetInternalObj(aAtkObj);
  if (!acc) {
    return;
  }

  mozilla::LayoutDeviceIntRect screenRect = acc->Bounds();
  if (screenRect.IsEmpty()) {
    return;
  }

  if (aCoordType == ATK_XY_WINDOW) {
    mozilla::LayoutDeviceIntPoint winCoords =
        nsAccUtils::GetScreenCoordsForWindow(acc);
    screenRect.x -= winCoords.x;
    screenRect.y -= winCoords.y;
  }

  double scaleFactor = getScaleFactor(acc);

  *aX = screenRect.x / scaleFactor;
  *aY = screenRect.y / scaleFactor;
  *aWidth = screenRect.width / scaleFactor;
  *aHeight = screenRect.height / scaleFactor;
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

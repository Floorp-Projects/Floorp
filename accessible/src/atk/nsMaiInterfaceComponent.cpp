/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InterfaceInitFuncs.h"

#include "nsAccessibleWrap.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsMai.h"

extern "C" {

static AtkObject*
refAccessibleAtPointCB(AtkComponent* aComponent, gint aAccX, gint aAccY,
                       AtkCoordType aCoordType)
{
  return refAccessibleAtPointHelper(GetAccessibleWrap(ATK_OBJECT(aComponent)),
                                    aAccX, aAccY, aCoordType);
}

static void
getExtentsCB(AtkComponent* aComponent, gint* aX, gint* aY,
             gint* aWidth, gint* aHeight, AtkCoordType aCoordType)
{
  getExtentsHelper(GetAccessibleWrap(ATK_OBJECT(aComponent)),
                   aX, aY, aWidth, aHeight, aCoordType);
}

static gboolean
grabFocusCB(AtkComponent* aComponent)
{
  nsAccessibleWrap* accWrap = GetAccessibleWrap(ATK_OBJECT(aComponent));
  if (!accWrap)
    return FALSE;

  nsresult rv = accWrap->TakeFocus();
  return (NS_FAILED(rv)) ? FALSE : TRUE;
}
}

AtkObject*
refAccessibleAtPointHelper(nsAccessibleWrap* aAccWrap, gint aX, gint aY,
                           AtkCoordType aCoordType)
{
  if (!aAccWrap || aAccWrap->IsDefunct() || nsAccUtils::MustPrune(aAccWrap))
    return nsnull;

  // nsAccessible::ChildAtPoint(x,y) is in screen pixels.
  if (aCoordType == ATK_XY_WINDOW) {
    nsIntPoint winCoords =
      nsCoreUtils::GetScreenCoordsForWindow(aAccWrap->GetNode());
    aX += winCoords.x;
    aY += winCoords.y;
  }

  nsAccessible* accAtPoint = aAccWrap->ChildAtPoint(aX, aY,
                                                    nsAccessible::eDirectChild);
  if (!accAtPoint)
    return nsnull;

  AtkObject* atkObj = nsAccessibleWrap::GetAtkObject(accAtPoint);
  if (atkObj)
    g_object_ref(atkObj);
  return atkObj;
}

void
getExtentsHelper(nsAccessibleWrap* aAccWrap,
                 gint* aX, gint* aY, gint* aWidth, gint* aHeight,
                 AtkCoordType aCoordType)
{
  *aX = *aY = *aWidth = *aHeight = 0;

  if (!aAccWrap || aAccWrap->IsDefunct())
    return;

  PRInt32 x = 0, y = 0, width = 0, height = 0;
  // Returned in screen coordinates
  nsresult rv = aAccWrap->GetBounds(&x, &y, &width, &height);
  if (NS_FAILED(rv))
    return;

  if (aCoordType == ATK_XY_WINDOW) {
    nsIntPoint winCoords =
      nsCoreUtils::GetScreenCoordsForWindow(aAccWrap->GetNode());
    x -= winCoords.x;
    y -= winCoords.y;
  }

  *aX = x;
  *aY = y;
  *aWidth = width;
  *aHeight = height;
}

void
componentInterfaceInitCB(AtkComponentIface* aIface)
{
  NS_ASSERTION(aIface, "Invalid Interface");
  if(NS_UNLIKELY(!aIface))
    return;

  /*
   * Use default implementation in atk for contains, get_position,
   * and get_size
   */
  aIface->ref_accessible_at_point = refAccessibleAtPointCB;
  aIface->get_extents = getExtentsCB;
  aIface->grab_focus = grabFocusCB;
}

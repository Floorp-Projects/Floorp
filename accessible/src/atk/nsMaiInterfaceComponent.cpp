/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

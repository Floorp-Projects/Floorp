/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleComponent.h"

#include "AccessibleComponent_i.c"

#include "nsAccessibleWrap.h"
#include "States.h"

#include "nsIFrame.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleComponent::QueryInterface(REFIID iid, void** ppv)
{
  *ppv = NULL;

  if (IID_IAccessibleComponent == iid) {
    *ppv = static_cast<IAccessibleComponent*>(this);
    (reinterpret_cast<IUnknown*>(*ppv))->AddRef();
    return S_OK;
  }

  return E_NOINTERFACE;
}

// IAccessibleComponent

STDMETHODIMP
ia2AccessibleComponent::get_locationInParent(long* aX, long* aY)
{
__try {
  *aX = 0;
  *aY = 0;

  nsAccessibleWrap* acc = static_cast<nsAccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // If the object is not on any screen the returned position is (0,0).
  PRUint64 state = acc->State();
  if (state & states::INVISIBLE)
    return S_OK;

  PRInt32 x = 0, y = 0, width = 0, height = 0;
  nsresult rv = acc->GetBounds(&x, &y, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  nsAccessible* parentAcc = acc->Parent();

  // The coordinates of the returned position are relative to this object's
  // parent or relative to the screen on which this object is rendered if it
  // has no parent.
  if (!parentAcc) {
    *aX = x;
    *aY = y;
    return S_OK;
  }

  // The coordinates of the bounding box are given relative to the parent's
  // coordinate system.
  PRInt32 parentx = 0, parenty = 0;
  rv = acc->GetBounds(&parentx, &parenty, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x - parentx;
  *aY = y - parenty;
  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }
  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleComponent::get_foreground(IA2Color* aForeground)
{
__try {
  nsAccessibleWrap* acc = static_cast<nsAccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame)
    *aForeground = frame->GetStyleColor()->mColor;

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}

STDMETHODIMP
ia2AccessibleComponent::get_background(IA2Color* aBackground)
{
__try {
  nsAccessibleWrap* acc = static_cast<nsAccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame)
    *aBackground = frame->GetStyleBackground()->mBackgroundColor;

  return S_OK;

} __except(nsAccessNodeWrap::FilterA11yExceptions(::GetExceptionCode(), GetExceptionInformation())) { }

  return E_FAIL;
}


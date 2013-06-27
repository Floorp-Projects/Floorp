/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ia2AccessibleComponent.h"

#include "AccessibleComponent_i.c"

#include "AccessibleWrap.h"
#include "States.h"
#include "IUnknownImpl.h"

#include "nsIFrame.h"

using namespace mozilla::a11y;

// IUnknown

STDMETHODIMP
ia2AccessibleComponent::QueryInterface(REFIID iid, void** ppv)
{
  if (!ppv)
    return E_INVALIDARG;

  *ppv = nullptr;

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
  A11Y_TRYBLOCK_BEGIN

  if (!aX || !aY)
    return E_INVALIDARG;

  *aX = 0;
  *aY = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  // If the object is not on any screen the returned position is (0,0).
  uint64_t state = acc->State();
  if (state & states::INVISIBLE)
    return S_OK;

  int32_t x = 0, y = 0, width = 0, height = 0;
  nsresult rv = acc->GetBounds(&x, &y, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  Accessible* parentAcc = acc->Parent();

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
  int32_t parentx = 0, parenty = 0;
  rv = acc->GetBounds(&parentx, &parenty, &width, &height);
  if (NS_FAILED(rv))
    return GetHRESULT(rv);

  *aX = x - parentx;
  *aY = y - parenty;
  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleComponent::get_foreground(IA2Color* aForeground)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aForeground)
    return E_INVALIDARG;

  *aForeground = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame)
    *aForeground = frame->StyleColor()->mColor;

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ia2AccessibleComponent::get_background(IA2Color* aBackground)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aBackground)
    return E_INVALIDARG;

  *aBackground = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame)
    *aBackground = frame->StyleBackground()->mBackgroundColor;

  return S_OK;

  A11Y_TRYBLOCK_END
}


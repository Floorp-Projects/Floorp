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
ia2AccessibleComponent::QueryInterface(REFIID iid, void** ppv) {
  if (!ppv) return E_INVALIDARG;

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
ia2AccessibleComponent::get_locationInParent(long* aX, long* aY) {
  if (!aX || !aY) return E_INVALIDARG;

  *aX = 0;
  *aY = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  // If the object is not on any screen the returned position is (0,0).
  uint64_t state = acc->State();
  if (state & states::INVISIBLE) return S_OK;

  nsIntRect rect = acc->Bounds();

  // The coordinates of the returned position are relative to this object's
  // parent or relative to the screen on which this object is rendered if it
  // has no parent.
  if (!acc->Parent()) {
    *aX = rect.X();
    *aY = rect.Y();
    return S_OK;
  }

  // The coordinates of the bounding box are given relative to the parent's
  // coordinate system.
  nsIntRect parentRect = acc->Parent()->Bounds();
  *aX = rect.X() - parentRect.X();
  *aY = rect.Y() - parentRect.Y();
  return S_OK;
}

STDMETHODIMP
ia2AccessibleComponent::get_foreground(IA2Color* aForeground) {
  if (!aForeground) return E_INVALIDARG;

  *aForeground = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame) *aForeground = frame->StyleColor()->mColor.ToColor();

  return S_OK;
}

STDMETHODIMP
ia2AccessibleComponent::get_background(IA2Color* aBackground) {
  if (!aBackground) return E_INVALIDARG;

  *aBackground = 0;

  AccessibleWrap* acc = static_cast<AccessibleWrap*>(this);
  if (acc->IsDefunct()) return CO_E_OBJNOTCONNECTED;

  nsIFrame* frame = acc->GetFrame();
  if (frame) {
    *aBackground = frame->StyleBackground()->BackgroundColor(frame);
  }

  return S_OK;
}

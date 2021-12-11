/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorPickerParent.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"

using mozilla::Unused;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(ColorPickerParent::ColorPickerShownCallback,
                  nsIColorPickerShownCallback);

NS_IMETHODIMP
ColorPickerParent::ColorPickerShownCallback::Update(const nsAString& aColor) {
  if (mColorPickerParent) {
    Unused << mColorPickerParent->SendUpdate(nsString(aColor));
  }
  return NS_OK;
}

NS_IMETHODIMP
ColorPickerParent::ColorPickerShownCallback::Done(const nsAString& aColor) {
  if (mColorPickerParent) {
    Unused << ColorPickerParent::Send__delete__(mColorPickerParent,
                                                nsString(aColor));
  }
  return NS_OK;
}

void ColorPickerParent::ColorPickerShownCallback::Destroy() {
  mColorPickerParent = nullptr;
}

bool ColorPickerParent::CreateColorPicker() {
  mPicker = do_CreateInstance("@mozilla.org/colorpicker;1");
  if (!mPicker) {
    return false;
  }

  Element* ownerElement = BrowserParent::GetFrom(Manager())->GetOwnerElement();
  if (!ownerElement) {
    return false;
  }

  nsCOMPtr<nsPIDOMWindowOuter> window = ownerElement->OwnerDoc()->GetWindow();
  if (!window) {
    return false;
  }

  return NS_SUCCEEDED(mPicker->Init(window, mTitle, mInitialColor));
}

mozilla::ipc::IPCResult ColorPickerParent::RecvOpen() {
  if (!CreateColorPicker()) {
    Unused << Send__delete__(this, mInitialColor);
    return IPC_OK();
  }

  mCallback = new ColorPickerShownCallback(this);

  mPicker->Open(mCallback);
  return IPC_OK();
};

void ColorPickerParent::ActorDestroy(ActorDestroyReason aWhy) {
  if (mCallback) {
    mCallback->Destroy();
  }
}

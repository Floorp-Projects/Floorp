/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set sw=4 ts=8 et tw=80 :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorPickerParent.h"
#include "nsComponentManagerUtils.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "mozilla/unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"

using mozilla::unused;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS1(ColorPickerParent::ColorPickerShownCallback,
                   nsIColorPickerShownCallback);

NS_IMETHODIMP
ColorPickerParent::ColorPickerShownCallback::Update(const nsAString& aColor)
{
  if (mColorPickerParent) {
    unused << mColorPickerParent->SendUpdate(nsString(aColor));
  }
  return NS_OK;
}

NS_IMETHODIMP
ColorPickerParent::ColorPickerShownCallback::Done(const nsAString& aColor)
{
  if (mColorPickerParent) {
    unused << mColorPickerParent->Send__delete__(mColorPickerParent,
                                                 nsString(aColor));
  }
  return NS_OK;
}

void
ColorPickerParent::ColorPickerShownCallback::Destroy()
{
  mColorPickerParent = nullptr;
}

bool
ColorPickerParent::CreateColorPicker()
{
  mPicker = do_CreateInstance("@mozilla.org/colorpicker;1");
  if (!mPicker) {
    return false;
  }

  Element* ownerElement = static_cast<TabParent*>(Manager())->GetOwnerElement();
  if (!ownerElement) {
    return false;
  }

  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(ownerElement->OwnerDoc()->GetWindow());
  if (!window) {
    return false;
  }

  return NS_SUCCEEDED(mPicker->Init(window, mTitle, mInitialColor));
}

bool
ColorPickerParent::RecvOpen()
{
  if (!CreateColorPicker()) {
    unused << Send__delete__(this, mInitialColor);
    return true;
  }

  mCallback = new ColorPickerShownCallback(this);

  mPicker->Open(mCallback);
  return true;
};

void
ColorPickerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCallback) {
    mCallback->Destroy();
  }
}

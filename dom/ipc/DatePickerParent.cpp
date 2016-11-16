/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DatePickerParent.h"
#include "nsComponentManagerUtils.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/TabParent.h"

using mozilla::Unused;
using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(DatePickerParent::DatePickerShownCallback,
                  nsIDatePickerShownCallback);

NS_IMETHODIMP
DatePickerParent::DatePickerShownCallback::Cancel()
{
  if (mDatePickerParent) {
    Unused << mDatePickerParent->SendCancel();
  }
  return NS_OK;
}

NS_IMETHODIMP
DatePickerParent::DatePickerShownCallback::Done(const nsAString& aDate)
{
  if (mDatePickerParent) {
    Unused << mDatePickerParent->Send__delete__(mDatePickerParent,
                                                nsString(aDate));
  }
  return NS_OK;
}

void
DatePickerParent::DatePickerShownCallback::Destroy()
{
  mDatePickerParent = nullptr;
}

bool
DatePickerParent::CreateDatePicker()
{
  mPicker = do_CreateInstance("@mozilla.org/datepicker;1");
  if (!mPicker) {
    return false;
  }

  Element* ownerElement = TabParent::GetFrom(Manager())->GetOwnerElement();
  if (!ownerElement) {
    return false;
  }

  nsCOMPtr<mozIDOMWindowProxy> window = do_QueryInterface(ownerElement->OwnerDoc()->GetWindow());
  if (!window) {
    return false;
  }

  return NS_SUCCEEDED(mPicker->Init(window, mTitle, mInitialDate));
}

mozilla::ipc::IPCResult
DatePickerParent::RecvOpen()
{
  if (!CreateDatePicker()) {
    Unused << Send__delete__(this, mInitialDate);
    return IPC_OK();
  }

  mCallback = new DatePickerShownCallback(this);

  mPicker->Open(mCallback);
  return IPC_OK();
};

void
DatePickerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCallback) {
    mCallback->Destroy();
  }
}

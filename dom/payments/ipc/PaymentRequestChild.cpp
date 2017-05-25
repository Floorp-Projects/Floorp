/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaymentRequestChild.h"
#include "mozilla/dom/PaymentRequestManager.h"

namespace mozilla {
namespace dom {

PaymentRequestChild::PaymentRequestChild()
  : mActorAlive(true)
{
}

nsresult
PaymentRequestChild::RequestPayment(const IPCPaymentActionRequest& aAction)
{
  if (!mActorAlive) {
    return NS_ERROR_FAILURE;
  }
  SendRequestPayment(aAction);
  return NS_OK;
}

void
PaymentRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorAlive = false;
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->ReleasePaymentChild(this);
}

void
PaymentRequestChild::MaybeDelete()
{
  if (mActorAlive) {
    mActorAlive = false;
    Send__delete__(this);
  }
}

bool
PaymentRequestChild::SendRequestPayment(const IPCPaymentActionRequest& aAction)
{
  return PPaymentRequestChild::SendRequestPayment(aAction);
}

} // end of namespace dom
} // end of namespace mozilla

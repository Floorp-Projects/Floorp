/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DetailedPromise.h"
#include "mozilla/dom/DOMException.h"

namespace mozilla {
namespace dom {

DetailedPromise::DetailedPromise(nsIGlobalObject* aGlobal)
  : Promise(aGlobal)
  , mResponded(false)
{
}

DetailedPromise::~DetailedPromise()
{
  MOZ_ASSERT(mResponded == IsPending());
}

static void
LogToConsole(const nsAString& aMsg)
{
  nsCOMPtr<nsIConsoleService> console(
    do_GetService("@mozilla.org/consoleservice;1"));
  if (!console) {
    NS_WARNING("Failed to log message to console.");
    return;
  }
  nsAutoString msg(aMsg);
  console->LogStringMessage(msg.get());
}

void
DetailedPromise::MaybeReject(nsresult aArg, const nsACString& aReason)
{
  mResponded = true;

  LogToConsole(NS_ConvertUTF8toUTF16(aReason));

  nsRefPtr<DOMException> exception =
    DOMException::Create(aArg, aReason);
  Promise::MaybeRejectBrokenly(exception);
}

void
DetailedPromise::MaybeReject(ErrorResult&, const nsACString& aReason)
{
  NS_NOTREACHED("nsresult expected in MaybeReject()");
}

/* static */ already_AddRefed<DetailedPromise>
DetailedPromise::Create(nsIGlobalObject* aGlobal, ErrorResult& aRv)
{
  nsRefPtr<DetailedPromise> promise = new DetailedPromise(aGlobal);
  promise->CreateWrapper(aRv);
  return aRv.Failed() ? nullptr : promise.forget();
}

} // namespace dom
} // namespace mozilla

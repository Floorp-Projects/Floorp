/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ProcessActor.h"

#include "nsContentUtils.h"
#include "mozilla/ContentBlockingAllowList.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/JSActorService.h"
#include "mozilla/dom/JSProcessActorParent.h"
#include "mozilla/dom/JSProcessActorChild.h"
#include "mozilla/dom/JSProcessActorProtocol.h"

namespace mozilla::dom {

already_AddRefed<JSActorProtocol> ProcessActor::MatchingJSActorProtocol(
    JSActorService* aActorSvc, const nsACString& aName, ErrorResult& aRv) {
  RefPtr<JSProcessActorProtocol> proto =
      aActorSvc->GetJSProcessActorProtocol(aName);
  if (!proto) {
    aRv.ThrowNotFoundError(nsPrintfCString("No such JSProcessActor '%s'",
                                           PromiseFlatCString(aName).get()));
    return nullptr;
  }

  if (!proto->Matches(GetRemoteType(), aRv)) {
    MOZ_ASSERT(aRv.Failed());
    return nullptr;
  }
  MOZ_ASSERT(!aRv.Failed());
  return proto.forget();
}

}  // namespace mozilla::dom

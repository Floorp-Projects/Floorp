/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientOpenWindowOpActors.h"

#include "ClientOpenWindowOpChild.h"
#include "mozilla/dom/PClientOpenWindowOpParent.h"

namespace mozilla {
namespace dom {

PClientOpenWindowOpChild* AllocClientOpenWindowOpChild() {
  return new ClientOpenWindowOpChild();
}

void InitClientOpenWindowOpChild(PClientOpenWindowOpChild* aActor,
                                 const ClientOpenWindowArgs& aArgs) {
  auto actor = static_cast<ClientOpenWindowOpChild*>(aActor);
  actor->Init(aArgs);
}

bool DeallocClientOpenWindowOpChild(PClientOpenWindowOpChild* aActor) {
  delete aActor;
  return true;
}

PClientOpenWindowOpParent* AllocClientOpenWindowOpParent(
    const ClientOpenWindowArgs& aArgs) {
  MOZ_CRASH("ClientOpenWindowOpParent must be explicitly allocated");
  return nullptr;
}

bool DeallocClientOpenWindowOpParent(PClientOpenWindowOpParent* aActor) {
  delete aActor;
  return true;
}

}  // namespace dom
}  // namespace mozilla

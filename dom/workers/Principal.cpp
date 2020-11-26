/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Principal.h"

#include "JSSettings.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/StructuredCloneTags.h"
#include "mozilla/dom/workerinternals/JSSettings.h"

namespace mozilla {
namespace dom {

WorkerPrincipal::WorkerPrincipal(bool aIsSystemOrAddonPrincipal)
    : JSPrincipals(), mIsSystemOrAddonPrincipal(aIsSystemOrAddonPrincipal) {
  setDebugToken(workerinternals::kJSPrincipalsDebugToken);
}

WorkerPrincipal::~WorkerPrincipal() = default;

bool WorkerPrincipal::write(JSContext* aCx, JSStructuredCloneWriter* aWriter) {
  return JS_WriteUint32Pair(aWriter, SCTAG_DOM_WORKER_PRINCIPAL, 0);
}

bool WorkerPrincipal::isSystemOrAddonPrincipal() {
  return mIsSystemOrAddonPrincipal;
}

void WorkerPrincipal::Destroy(JSPrincipals* aPrincipals) {
  delete static_cast<WorkerPrincipal*>(aPrincipals);
}

}  // namespace dom
}  // namespace mozilla

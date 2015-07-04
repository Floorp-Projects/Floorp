/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadMonitoring.h"
#include "mozilla/dom/GamepadFunctions.h"
#include "mozilla/dom/PContentParent.h"

namespace mozilla {
namespace dom {

using namespace GamepadFunctions;

void
MaybeStopGamepadMonitoring()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  nsTArray<ContentParent*> t;
  ContentParent::GetAll(t);
  for(uint32_t i = 0; i < t.Length(); ++i) {
    if (t[i]->HasGamepadListener()) {
      return;
    }
  }
  StopGamepadMonitoring();
  ResetGamepadIndexes();
}

} // namespace dom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintWorkletGlobalScope.h"
#include "WorkletPrincipal.h"
#include "mozilla/dom/PaintWorkletGlobalScopeBinding.h"
#include "mozilla/dom/FunctionBinding.h"

namespace mozilla {
namespace dom {

PaintWorkletGlobalScope::PaintWorkletGlobalScope() = default;

bool
PaintWorkletGlobalScope::WrapGlobalObject(JSContext* aCx,
                                          JS::MutableHandle<JSObject*> aReflector)
{
  JS::RealmOptions options;
  return PaintWorkletGlobalScopeBinding::Wrap(aCx, this, this,
                                              options,
                                              WorkletPrincipal::GetWorkletPrincipal(),
                                              true, aReflector);
}

void
PaintWorkletGlobalScope::RegisterPaint(const nsAString& aType,
                                       VoidFunction& aProcessorCtor)
{
  // Nothing to do here, yet.
}

} // dom namespace
} // mozilla namespace

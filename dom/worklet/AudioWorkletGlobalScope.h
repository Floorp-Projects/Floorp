/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioWorkletGlobalScope_h
#define mozilla_dom_AudioWorkletGlobalScope_h

#include "mozilla/dom/WorkletGlobalScope.h"

namespace mozilla {
namespace dom {

class VoidFunction;

class AudioWorkletGlobalScope final : public WorkletGlobalScope
{
public:
  explicit AudioWorkletGlobalScope(nsPIDOMWindowInner* aWindow);

  bool
  WrapGlobalObject(JSContext* aCx, nsIPrincipal* aPrincipal,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void
  RegisterProcessor(const nsAString& aType,
                    VoidFunction& aProcessorCtor);

private:
  ~AudioWorkletGlobalScope();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AudioWorkletGlobalScope_h

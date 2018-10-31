/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaintWorkletGlobalScope_h
#define mozilla_dom_PaintWorkletGlobalScope_h

#include "mozilla/dom/WorkletGlobalScope.h"

namespace mozilla {

class PaintWorkletImpl;

namespace dom {

class VoidFunction;

class PaintWorkletGlobalScope final : public WorkletGlobalScope
{
public:
  explicit PaintWorkletGlobalScope(PaintWorkletImpl* aImpl);

  bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void
  RegisterPaint(const nsAString& aType, VoidFunction& aProcessorCtor);

  WorkletImpl* Impl() const override;

private:
  ~PaintWorkletGlobalScope() = default;

  const RefPtr<PaintWorkletImpl> mImpl;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PaintWorkletGlobalScope_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef PaintWorkletImpl_h
#define PaintWorkletImpl_h

#include "mozilla/dom/WorkletImpl.h"

namespace mozilla {

class PaintWorkletImpl final : public WorkletImpl
{
public:
  // Methods for parent thread only:

  static already_AddRefed<dom::Worklet>
  CreateWorklet(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal);

protected:
  // Execution thread only.
  already_AddRefed<dom::WorkletGlobalScope> ConstructGlobalScope() override;

private:
  PaintWorkletImpl(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal);
  ~PaintWorkletImpl();
};

} // namespace mozilla

#endif // PaintWorkletImpl_h

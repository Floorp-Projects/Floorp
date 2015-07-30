/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSRENDERINGCONTEXT__H__
#define NSRENDERINGCONTEXT__H__

#include "gfxContext.h"
#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "mozilla/nsRefPtr.h"

namespace mozilla {
namespace gfx {
class DrawTarget;
} // namespace gfx
} // namespace mozilla

class MOZ_STACK_CLASS nsRenderingContext final
{
    typedef mozilla::gfx::DrawTarget DrawTarget;

public:
    nsRenderingContext() {}

    explicit nsRenderingContext(gfxContext* aThebesContext)
      : mThebes(aThebesContext)
    {}

    explicit nsRenderingContext(already_AddRefed<gfxContext>&& aThebesContext)
      : mThebes(aThebesContext)
    {}

    explicit nsRenderingContext(DrawTarget* aDrawTarget) {
      Init(aDrawTarget);
    }

    void Init(gfxContext* aThebesContext);
    void Init(DrawTarget* aDrawTarget);

    // These accessors will never return null.
    gfxContext *ThebesContext() { return mThebes; }
    DrawTarget *GetDrawTarget() { return mThebes->GetDrawTarget(); }

private:
    nsRefPtr<gfxContext> mThebes;
};

#endif  // NSRENDERINGCONTEXT__H__

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/ActivationContext.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"

namespace mozilla {
namespace mscom {

ActivationContext::ActivationContext(HMODULE aLoadFromModule)
  : mActCtx(INVALID_HANDLE_VALUE)
  , mActivationCookie(0)
{
  ACTCTX actCtx = {sizeof(actCtx)};
  actCtx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
  actCtx.lpResourceName = MAKEINTRESOURCE(2);
  actCtx.hModule = aLoadFromModule;

  mActCtx = ::CreateActCtx(&actCtx);
  MOZ_ASSERT(mActCtx != INVALID_HANDLE_VALUE);
  if (mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }
  if (!::ActivateActCtx(mActCtx, &mActivationCookie)) {
    ::ReleaseActCtx(mActCtx);
    mActCtx = INVALID_HANDLE_VALUE;
  }
}

ActivationContext::~ActivationContext()
{
  if (mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }
  DebugOnly<BOOL> deactivated = ::DeactivateActCtx(0, mActivationCookie);
  MOZ_ASSERT(deactivated);
  ::ReleaseActCtx(mActCtx);
}

} // namespace mscom
} // namespace mozilla

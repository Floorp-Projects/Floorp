/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/ActivationContext.h"

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/Utils.h"

namespace mozilla {
namespace mscom {

ActivationContext::ActivationContext(WORD aResourceId)
  : ActivationContext(reinterpret_cast<HMODULE>(GetContainingModuleHandle()),
                      aResourceId)
{
}

ActivationContext::ActivationContext(HMODULE aLoadFromModule, WORD aResourceId)
  : mActCtx(INVALID_HANDLE_VALUE)
{
  ACTCTX actCtx = {sizeof(actCtx)};
  actCtx.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
  actCtx.lpResourceName = MAKEINTRESOURCE(aResourceId);
  actCtx.hModule = aLoadFromModule;

  Init(actCtx);
}

void
ActivationContext::Init(ACTCTX& aActCtx)
{
  MOZ_ASSERT(mActCtx == INVALID_HANDLE_VALUE);
  mActCtx = ::CreateActCtx(&aActCtx);
  MOZ_ASSERT(mActCtx != INVALID_HANDLE_VALUE);
}

void
ActivationContext::AddRef()
{
  if (mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }
  ::AddRefActCtx(mActCtx);
}

ActivationContext::ActivationContext(ActivationContext&& aOther)
  : mActCtx(aOther.mActCtx)
{
  aOther.mActCtx = INVALID_HANDLE_VALUE;
}

ActivationContext&
ActivationContext::operator=(ActivationContext&& aOther)
{
  Release();

  mActCtx = aOther.mActCtx;
  aOther.mActCtx = INVALID_HANDLE_VALUE;
  return *this;
}

ActivationContext::ActivationContext(const ActivationContext& aOther)
  : mActCtx(aOther.mActCtx)
{
  AddRef();
}

ActivationContext&
ActivationContext::operator=(const ActivationContext& aOther)
{
  Release();
  mActCtx = aOther.mActCtx;
  AddRef();
  return *this;
}

void
ActivationContext::Release()
{
  if (mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }
  ::ReleaseActCtx(mActCtx);
  mActCtx = INVALID_HANDLE_VALUE;
}

ActivationContext::~ActivationContext()
{
  Release();
}

ActivationContextRegion::ActivationContextRegion(const ActivationContext& aActCtx)
  : mActCtx(aActCtx)
  , mActCookie(0)
{
  if (aActCtx.mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }

  DebugOnly<BOOL> activated = ::ActivateActCtx(aActCtx.mActCtx, &mActCookie);
  MOZ_ASSERT(activated);
}

ActivationContextRegion::ActivationContextRegion(ActivationContext&& aActCtx)
  : mActCtx(Move(aActCtx))
  , mActCookie(0)
{
  if (aActCtx.mActCtx == INVALID_HANDLE_VALUE) {
    return;
  }

  DebugOnly<BOOL> activated = ::ActivateActCtx(aActCtx.mActCtx, &mActCookie);
  MOZ_ASSERT(activated);
}

ActivationContextRegion::~ActivationContextRegion()
{
  if (!mActCookie) {
    return;
  }

  DebugOnly<BOOL> deactivated = ::DeactivateActCtx(0, mActCookie);
  MOZ_ASSERT(deactivated);
}

} // namespace mscom
} // namespace mozilla

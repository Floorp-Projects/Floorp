/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ActivationContext_h
#define mozilla_mscom_ActivationContext_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/Result.h"

#if defined(MOZILLA_INTERNAL_API)
#include "nsString.h"
#endif // defined(MOZILLA_INTERNAL_API)

#include <windows.h>

namespace mozilla {
namespace mscom {

class ActivationContext final
{
public:
  ActivationContext()
    : mActCtx(INVALID_HANDLE_VALUE)
  {
  }

  explicit ActivationContext(WORD aResourceId);
  explicit ActivationContext(HMODULE aLoadFromModule, WORD aResourceId = 2);

  ActivationContext(ActivationContext&& aOther);
  ActivationContext& operator=(ActivationContext&& aOther);

  ActivationContext(const ActivationContext& aOther);
  ActivationContext& operator=(const ActivationContext& aOther);

  ~ActivationContext();

  explicit operator bool() const
  {
    return mActCtx != INVALID_HANDLE_VALUE;
  }

#if defined(MOZILLA_INTERNAL_API)
  static Result<uintptr_t,HRESULT> GetCurrent();
  static HRESULT GetCurrentManifestPath(nsAString& aOutManifestPath);
#endif // defined(MOZILLA_INTERNAL_API)

private:
  void Init(ACTCTX& aActCtx);
  void AddRef();
  void Release();

private:
  HANDLE mActCtx;

  friend class ActivationContextRegion;
};

class MOZ_NON_TEMPORARY_CLASS ActivationContextRegion final
{
public:
  template <typename... Args>
  explicit ActivationContextRegion(Args... aArgs)
    : mActCtx(std::forward<Args>(aArgs)...)
    , mActCookie(0)
  {
    Activate();
  }

  ActivationContextRegion();

  explicit ActivationContextRegion(const ActivationContext& aActCtx);
  ActivationContextRegion& operator=(const ActivationContext& aActCtx);

  explicit ActivationContextRegion(ActivationContext&& aActCtx);
  ActivationContextRegion& operator=(ActivationContext&& aActCtx);

  ActivationContextRegion(ActivationContextRegion&& aRgn);
  ActivationContextRegion& operator=(ActivationContextRegion&& aRgn);

  ~ActivationContextRegion();

  explicit operator bool() const
  {
    return !!mActCookie;
  }

  ActivationContextRegion(const ActivationContextRegion&) = delete;
  ActivationContextRegion& operator=(const ActivationContextRegion&) = delete;

  bool Deactivate();

private:
  void Activate();

  ActivationContext mActCtx;
  ULONG_PTR         mActCookie;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_ActivationContext_h


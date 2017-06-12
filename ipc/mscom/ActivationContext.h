/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_ActivationContext_h
#define mozilla_mscom_ActivationContext_h

#include "mozilla/Attributes.h"

#include <windows.h>

namespace mozilla {
namespace mscom {

class MOZ_RAII ActivationContext
{
public:
  explicit ActivationContext(HMODULE aLoadFromModule);
  ~ActivationContext();

  explicit operator bool() const
  {
    return mActCtx != INVALID_HANDLE_VALUE;
  }

  ActivationContext(const ActivationContext&) = delete;
  ActivationContext(ActivationContext&&) = delete;
  ActivationContext& operator=(const ActivationContext&) = delete;
  ActivationContext& operator=(ActivationContext&&) = delete;

private:
  HANDLE    mActCtx;
  ULONG_PTR mActivationCookie;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_ActivationContext_h


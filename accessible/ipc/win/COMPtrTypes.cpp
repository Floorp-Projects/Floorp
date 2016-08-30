/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/COMPtrTypes.h"

#include "MainThreadUtils.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/MainThreadHandoff.h"
#include "mozilla/RefPtr.h"

using mozilla::mscom::MainThreadHandoff;
using mozilla::mscom::STAUniquePtr;

namespace mozilla {
namespace a11y {

IAccessibleHolder
CreateHolderFromAccessible(Accessible* aAccToWrap)
{
  MOZ_ASSERT(aAccToWrap && NS_IsMainThread());
  if (!aAccToWrap) {
    return nullptr;
  }

  IAccessible* rawNative = nullptr;
  aAccToWrap->GetNativeInterface((void**)&rawNative);
  MOZ_ASSERT(rawNative);
  if (!rawNative) {
    return nullptr;
  }

  STAUniquePtr<IAccessible> iaToProxy(rawNative);

  IAccessible* rawIntercepted = nullptr;
  HRESULT hr = MainThreadHandoff::WrapInterface(Move(iaToProxy), &rawIntercepted);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return nullptr;
  }

  IAccessibleHolder::COMPtrType iaIntercepted(rawIntercepted);
  return IAccessibleHolder(Move(iaIntercepted));
}

} // namespace a11y
} // namespace mozilla

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/COMPtrTypes.h"

#include <utility>

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "mozilla/a11y/LocalAccessible.h"
#include "mozilla/a11y/HandlerProvider.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/mscom/MainThreadHandoff.h"
#include "mozilla/mscom/Utils.h"
#include "nsXULAppAPI.h"

using mozilla::mscom::MainThreadHandoff;
using mozilla::mscom::ProxyUniquePtr;
using mozilla::mscom::STAUniquePtr;

namespace mozilla {
namespace a11y {

IAccessibleHolder CreateHolderFromAccessible(
    NotNull<LocalAccessible*> aAccToWrap) {
  MOZ_ASSERT(NS_IsMainThread());

  STAUniquePtr<IAccessible> iaToProxy;
  aAccToWrap->GetNativeInterface(mscom::getter_AddRefs(iaToProxy));
  MOZ_DIAGNOSTIC_ASSERT(iaToProxy);
  if (!iaToProxy) {
    return nullptr;
  }

  static const bool useHandler =
      Preferences::GetBool("accessibility.handler.enabled", false) &&
      IsHandlerRegistered();

  RefPtr<HandlerProvider> payload;
  if (useHandler) {
    payload = new HandlerProvider(IID_IAccessible,
                                  mscom::ToInterceptorTargetPtr(iaToProxy));
  }

  ProxyUniquePtr<IAccessible> intercepted;
  HRESULT hr = MainThreadHandoff::WrapInterface(
      std::move(iaToProxy), payload,
      (IAccessible**)mscom::getter_AddRefs(intercepted));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return nullptr;
  }

  return IAccessibleHolder(std::move(intercepted));
}

IHandlerControlHolder CreateHolderFromHandlerControl(
    mscom::ProxyUniquePtr<IHandlerControl> aHandlerControl) {
  MOZ_ASSERT(aHandlerControl);
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (!aHandlerControl) {
    return nullptr;
  }

  return IHandlerControlHolder(std::move(aHandlerControl));
}

}  // namespace a11y
}  // namespace mozilla

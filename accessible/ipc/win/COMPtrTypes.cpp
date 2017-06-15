/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/COMPtrTypes.h"

#include "Accessible2_3.h"
#include "MainThreadUtils.h"
#include "mozilla/a11y/Accessible.h"
#include "mozilla/a11y/Platform.h"
#include "mozilla/a11y/HandlerProvider.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/MainThreadHandoff.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Preferences.h"
#include "mozilla/RefPtr.h"
#include "nsXULAppAPI.h"

using mozilla::mscom::MainThreadHandoff;
using mozilla::mscom::ProxyUniquePtr;
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

  STAUniquePtr<IAccessible> iaToProxy;
  aAccToWrap->GetNativeInterface(mscom::getter_AddRefs(iaToProxy));
  MOZ_ASSERT(iaToProxy);
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
  HRESULT hr = MainThreadHandoff::WrapInterface(Move(iaToProxy), payload,
                                                (IAccessible**) mscom::getter_AddRefs(intercepted));
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return nullptr;
  }

  return IAccessibleHolder(Move(intercepted));
}

IHandlerControlHolder
CreateHolderFromHandlerControl(mscom::ProxyUniquePtr<IHandlerControl> aHandlerControl)
{
  MOZ_ASSERT(aHandlerControl);
  MOZ_ASSERT(XRE_IsContentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  if (!aHandlerControl) {
    return nullptr;
  }

  return IHandlerControlHolder(Move(aHandlerControl));
}

} // namespace a11y
} // namespace mozilla

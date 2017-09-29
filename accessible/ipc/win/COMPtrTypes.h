/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_COMPtrTypes_h
#define mozilla_a11y_COMPtrTypes_h

#include "mozilla/a11y/AccessibleHandler.h"
#include "mozilla/a11y/Compatibility.h"
#include "mozilla/Attributes.h"
#include "mozilla/mscom/ActivationContext.h"
#include "mozilla/mscom/COMPtrHolder.h"
#include "mozilla/NotNull.h"

#include <oleacc.h>

namespace mozilla {
namespace a11y {

class MOZ_RAII IAccessibleEnvironment : public mscom::ProxyStream::Environment
{
public:
  IAccessibleEnvironment() = default;

  bool Push() override
  {
    mActCtxRgn = GetActCtx();
    return !!mActCtxRgn;
  }

  bool Pop() override
  {
    return mActCtxRgn.Deactivate();
  }

private:
  static const mscom::ActivationContext& GetActCtx()
  {
    static const mscom::ActivationContext
      sActCtx(Compatibility::GetActCtxResourceId());
    MOZ_DIAGNOSTIC_ASSERT(sActCtx);
    return sActCtx;
  }

private:
  mscom::ActivationContextRegion mActCtxRgn;
};

} // namespace a11y

namespace mscom {
namespace detail {

template<>
struct EnvironmentSelector<IAccessible>
{
  typedef a11y::IAccessibleEnvironment Type;
};

} // namespace detail
} // namespace mscom

namespace a11y {

typedef mozilla::mscom::COMPtrHolder<IAccessible, IID_IAccessible> IAccessibleHolder;
typedef mozilla::mscom::COMPtrHolder<IDispatch, IID_IDispatch> IDispatchHolder;

class Accessible;

IAccessibleHolder
CreateHolderFromAccessible(NotNull<Accessible*> aAccToWrap);

typedef mozilla::mscom::COMPtrHolder<IHandlerControl, IID_IHandlerControl> IHandlerControlHolder;

IHandlerControlHolder
CreateHolderFromHandlerControl(mscom::ProxyUniquePtr<IHandlerControl> aHandlerControl);

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_COMPtrTypes_h

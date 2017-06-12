/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/PlatformChild.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/InterceptorLog.h"

#include "Accessible2.h"
#include "Accessible2_2.h"
#include "AccessibleHypertext2.h"
#include "AccessibleTableCell.h"

#include "AccessibleHypertext2_i.c"

namespace mozilla {
namespace a11y {

/**
 * Unfortunately the COM interceptor does not intrinsically handle array
 * outparams. Instead we manually define the relevant metadata here, and
 * register it in a call to mozilla::mscom::RegisterArrayData.
 * @see mozilla::mscom::ArrayData
 */
static const mozilla::mscom::ArrayData sPlatformChildArrayData[] = {
  {IID_IEnumVARIANT, 3, 1, VT_DISPATCH, IID_IDispatch, 2},
  {IID_IAccessible2, 30, 1, VT_UNKNOWN | VT_BYREF, IID_IAccessibleRelation, 2},
  {IID_IAccessibleRelation, 7, 1, VT_UNKNOWN | VT_BYREF, IID_IUnknown, 2},
  {IID_IAccessible2_2, 48, 2, VT_UNKNOWN | VT_BYREF, IID_IUnknown, 3,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleTableCell, 4, 0, VT_UNKNOWN | VT_BYREF, IID_IUnknown, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleTableCell, 7, 0, VT_UNKNOWN | VT_BYREF, IID_IUnknown, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleHypertext2, 25, 0, VT_UNKNOWN | VT_BYREF, IID_IUnknown, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer}
};

// Type libraries are thread-neutral, so we can register those from any
// apartment. OTOH, proxies must be registered from within the apartment where
// we intend to instantiate them. Therefore RegisterProxy() must be called
// via EnsureMTA.
PlatformChild::PlatformChild()
  : mAccTypelib(mozilla::mscom::RegisterTypelib(L"oleacc.dll",
        mozilla::mscom::RegistrationFlags::eUseSystemDirectory))
  , mMiscTypelib(mozilla::mscom::RegisterTypelib(L"Accessible.tlb"))
  , mSdnTypelib(mozilla::mscom::RegisterTypelib(L"AccessibleMarshal.dll"))
{
  mozilla::mscom::InterceptorLog::Init();
  mozilla::mscom::RegisterArrayData(sPlatformChildArrayData);


  UniquePtr<mozilla::mscom::RegisteredProxy> customProxy;
  mozilla::mscom::EnsureMTA([&customProxy]() -> void {
    customProxy = Move(mozilla::mscom::RegisterProxy());
  });
  mCustomProxy = Move(customProxy);

  UniquePtr<mozilla::mscom::RegisteredProxy> ia2Proxy;
  mozilla::mscom::EnsureMTA([&ia2Proxy]() -> void {
    ia2Proxy = Move(mozilla::mscom::RegisterProxy(L"ia2marshal.dll"));
  });
  mIA2Proxy = Move(ia2Proxy);
}

} // namespace a11y
} // namespace mozilla


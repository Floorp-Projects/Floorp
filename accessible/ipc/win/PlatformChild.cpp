/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/a11y/AccessibleHandler.h"
#include "mozilla/a11y/Compatibility.h"
#include "mozilla/a11y/PlatformChild.h"
#include "mozilla/mscom/EnsureMTA.h"
#include "mozilla/mscom/InterceptorLog.h"

#include "Accessible2.h"
#include "Accessible2_2.h"
#include "AccessibleHypertext2.h"
#include "AccessibleTable2.h"
#include "AccessibleTableCell.h"

#include "AccessibleDocument_i.c"
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
  {IID_IAccessibleRelation, 7, 1, VT_UNKNOWN | VT_BYREF, NEWEST_IA2_IID, 2},
  {IID_IAccessible2_2, 48, 2, VT_UNKNOWN | VT_BYREF, NEWEST_IA2_IID, 3,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleTableCell, 4, 0, VT_UNKNOWN | VT_BYREF, NEWEST_IA2_IID, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleTableCell, 7, 0, VT_UNKNOWN | VT_BYREF, NEWEST_IA2_IID, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleHypertext2, 25, 0, VT_UNKNOWN | VT_BYREF,
   IID_IAccessibleHyperlink, 1,
   mozilla::mscom::ArrayData::Flag::eAllocatedByServer},
  {IID_IAccessibleTable2, 12, 0, VT_UNKNOWN | VT_BYREF,
   NEWEST_IA2_IID, 1, mozilla::mscom::ArrayData::Flag::eAllocatedByServer}
};

// Type libraries are thread-neutral, so we can register those from any
// apartment. OTOH, proxies must be registered from within the apartment where
// we intend to instantiate them. Therefore RegisterProxy() must be called
// via EnsureMTA.
PlatformChild::PlatformChild()
  : mIA2Proxy(mozilla::mscom::RegisterProxy(L"ia2marshal.dll"))
  , mAccTypelib(mozilla::mscom::RegisterTypelib(L"oleacc.dll",
        mozilla::mscom::RegistrationFlags::eUseSystemDirectory))
  , mMiscTypelib(mozilla::mscom::RegisterTypelib(L"Accessible.tlb"))
  , mSdnTypelib(mozilla::mscom::RegisterTypelib(L"AccessibleMarshal.dll"))
{
  WORD actCtxResourceId = Compatibility::GetActCtxResourceId();

  mozilla::mscom::MTADeletePtr<mozilla::mscom::ActivationContextRegion> tmpActCtxMTA;
  mozilla::mscom::EnsureMTA([actCtxResourceId, &tmpActCtxMTA]() -> void {
    tmpActCtxMTA.reset(new mozilla::mscom::ActivationContextRegion(actCtxResourceId));
  });
  mActCtxMTA = std::move(tmpActCtxMTA);

  mozilla::mscom::InterceptorLog::Init();
  mozilla::mscom::RegisterArrayData(sPlatformChildArrayData);


  UniquePtr<mozilla::mscom::RegisteredProxy> customProxy;
  mozilla::mscom::EnsureMTA([&customProxy]() -> void {
    customProxy = std::move(mozilla::mscom::RegisterProxy());
  });
  mCustomProxy = std::move(customProxy);

  // IA2 needs to be registered in both the main thread's STA as well as the MTA
  UniquePtr<mozilla::mscom::RegisteredProxy> ia2ProxyMTA;
  mozilla::mscom::EnsureMTA([&ia2ProxyMTA]() -> void {
    ia2ProxyMTA = std::move(mozilla::mscom::RegisterProxy(L"ia2marshal.dll"));
  });
  mIA2ProxyMTA = std::move(ia2ProxyMTA);
}

} // namespace a11y
} // namespace mozilla


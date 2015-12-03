/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "D3DMessageUtils.h"
#if defined(XP_WIN)
# include "gfxWindowsPlatform.h"
#endif

bool
DxgiAdapterDesc::operator ==(const DxgiAdapterDesc& aOther) const
{
  return memcmp(&aOther, this, sizeof(*this)) == 0;
}

#if defined(XP_WIN)
static_assert(sizeof(DxgiAdapterDesc) == sizeof(DXGI_ADAPTER_DESC),
              "DXGI_ADAPTER_DESC doe snot match DxgiAdapterDesc");

const DxgiAdapterDesc&
DxgiAdapterDesc::From(const DXGI_ADAPTER_DESC& aDesc)
{
  return reinterpret_cast<const DxgiAdapterDesc&>(aDesc);
}

const DXGI_ADAPTER_DESC&
DxgiAdapterDesc::ToDesc() const
{
  return reinterpret_cast<const DXGI_ADAPTER_DESC&>(*this);
}
#endif

namespace IPC {

void
ParamTraits<DxgiAdapterDesc>::Write(Message* aMsg, const paramType& aParam)
{
#if defined(XP_WIN)
  aMsg->WriteBytes(aParam.Description, sizeof(aParam.Description));
  WriteParam(aMsg, aParam.VendorId);
  WriteParam(aMsg, aParam.DeviceId);
  WriteParam(aMsg, aParam.SubSysId);
  WriteParam(aMsg, aParam.Revision);
  WriteParam(aMsg, aParam.DedicatedVideoMemory);
  WriteParam(aMsg, aParam.DedicatedSystemMemory);
  WriteParam(aMsg, aParam.SharedSystemMemory);
  WriteParam(aMsg, aParam.AdapterLuid.LowPart);
  WriteParam(aMsg, aParam.AdapterLuid.HighPart);
#else
  MOZ_ASSERT_UNREACHABLE("DxgiAdapterDesc is Windows-only");
#endif
}

bool
ParamTraits<DxgiAdapterDesc>::Read(const Message* aMsg, void** aIter, paramType* aResult)
{
#if defined(XP_WIN)
  const char* description = nullptr;
  if (!aMsg->ReadBytes(aIter, &description, sizeof(aResult->Description))) {
    return false;
  }
  memcpy(aResult->Description, description, sizeof(aResult->Description));

  if (ReadParam(aMsg, aIter, &aResult->VendorId) &&
      ReadParam(aMsg, aIter, &aResult->DeviceId) &&
      ReadParam(aMsg, aIter, &aResult->SubSysId) &&
      ReadParam(aMsg, aIter, &aResult->Revision) &&
      ReadParam(aMsg, aIter, &aResult->DedicatedVideoMemory) &&
      ReadParam(aMsg, aIter, &aResult->DedicatedSystemMemory) &&
      ReadParam(aMsg, aIter, &aResult->SharedSystemMemory) &&
      ReadParam(aMsg, aIter, &aResult->AdapterLuid.LowPart) &&
      ReadParam(aMsg, aIter, &aResult->AdapterLuid.HighPart))
  {
    return true;
  }
#else
  MOZ_ASSERT_UNREACHABLE("DxgiAdapterDesc is Windows-only");
#endif
  return false;
}

} // namespace IPC

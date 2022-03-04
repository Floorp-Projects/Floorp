/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "D3DMessageUtils.h"
#if defined(XP_WIN)
#  include "gfxWindowsPlatform.h"
#endif

bool DxgiAdapterDesc::operator==(const DxgiAdapterDesc& aOther) const {
  return memcmp(&aOther, this, sizeof(*this)) == 0;
}

#if defined(XP_WIN)
static_assert(sizeof(DxgiAdapterDesc) == sizeof(DXGI_ADAPTER_DESC),
              "DXGI_ADAPTER_DESC doe snot match DxgiAdapterDesc");

const DxgiAdapterDesc& DxgiAdapterDesc::From(const DXGI_ADAPTER_DESC& aDesc) {
  return reinterpret_cast<const DxgiAdapterDesc&>(aDesc);
}

const DXGI_ADAPTER_DESC& DxgiAdapterDesc::ToDesc() const {
  return reinterpret_cast<const DXGI_ADAPTER_DESC&>(*this);
}
#endif

namespace IPC {

void ParamTraits<DxgiAdapterDesc>::Write(MessageWriter* aWriter,
                                         const paramType& aParam) {
#if defined(XP_WIN)
  aWriter->WriteBytes(aParam.Description, sizeof(aParam.Description));
  WriteParam(aWriter, aParam.VendorId);
  WriteParam(aWriter, aParam.DeviceId);
  WriteParam(aWriter, aParam.SubSysId);
  WriteParam(aWriter, aParam.Revision);
  WriteParam(aWriter, aParam.DedicatedVideoMemory);
  WriteParam(aWriter, aParam.DedicatedSystemMemory);
  WriteParam(aWriter, aParam.SharedSystemMemory);
  WriteParam(aWriter, aParam.AdapterLuid.LowPart);
  WriteParam(aWriter, aParam.AdapterLuid.HighPart);
#endif
}

bool ParamTraits<DxgiAdapterDesc>::Read(MessageReader* aReader,
                                        paramType* aResult) {
#if defined(XP_WIN)
  if (!aReader->ReadBytesInto(aResult->Description,
                              sizeof(aResult->Description))) {
    return false;
  }

  if (ReadParam(aReader, &aResult->VendorId) &&
      ReadParam(aReader, &aResult->DeviceId) &&
      ReadParam(aReader, &aResult->SubSysId) &&
      ReadParam(aReader, &aResult->Revision) &&
      ReadParam(aReader, &aResult->DedicatedVideoMemory) &&
      ReadParam(aReader, &aResult->DedicatedSystemMemory) &&
      ReadParam(aReader, &aResult->SharedSystemMemory) &&
      ReadParam(aReader, &aResult->AdapterLuid.LowPart) &&
      ReadParam(aReader, &aResult->AdapterLuid.HighPart)) {
    return true;
  }
  return false;
#else
  return true;
#endif
}

}  // namespace IPC

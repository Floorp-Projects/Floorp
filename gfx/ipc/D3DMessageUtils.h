/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_gfx_ipc_D3DMessageUtils_h__
#define _include_gfx_ipc_D3DMessageUtils_h__

#include "chrome/common/ipc_message_utils.h"
#include "ipc/IPCMessageUtils.h"

// Can't include dxgi.h, since it leaks random identifiers and #defines, and
// IPDL causes this file to be #included all over.
typedef struct DXGI_ADAPTER_DESC DXGI_ADAPTER_DESC;

struct DxgiAdapterDesc
{
#if defined(XP_WIN)
    WCHAR Description[128];
    UINT VendorId;
    UINT DeviceId;
    UINT SubSysId;
    UINT Revision;
    SIZE_T DedicatedVideoMemory;
    SIZE_T DedicatedSystemMemory;
    SIZE_T SharedSystemMemory;
    LUID AdapterLuid;

    static const DxgiAdapterDesc& From(const DXGI_ADAPTER_DESC& aDesc);
    const DXGI_ADAPTER_DESC& ToDesc() const;
#endif

    bool operator ==(const DxgiAdapterDesc& aOther) const;
};

namespace IPC {

template <>
struct ParamTraits<DxgiAdapterDesc>
{
  typedef DxgiAdapterDesc paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};

} // namespace IPC

#endif // _include_gfx_ipc_D3DMessageUtils_h__

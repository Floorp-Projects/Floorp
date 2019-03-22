/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// COM registration data structures are built with C code, so we need to
// simulate that in our C++ code by defining CINTERFACE before including
// anything else that could possibly pull in Windows header files.
#define CINTERFACE

#include "mozilla/mscom/ActivationContext.h"
#include "mozilla/mscom/Registration.h"
#include "mozilla/mscom/Utils.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/Pair.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/Vector.h"
#include "nsWindowsHelpers.h"

#if defined(MOZILLA_INTERNAL_API)
#  include "mozilla/ClearOnShutdown.h"
#  include "mozilla/mscom/EnsureMTA.h"
HRESULT RegisterPassthruProxy();
#else
#  include <stdlib.h>
#endif  // defined(MOZILLA_INTERNAL_API)

#include <oaidl.h>
#include <objidl.h>
#include <rpcproxy.h>
#include <shlwapi.h>

#include <algorithm>

/* This code MUST NOT use any non-inlined internal Mozilla APIs, as it will be
   compiled into DLLs that COM may load into non-Mozilla processes! */

extern "C" {

// This function is defined in generated code for proxy DLLs but is not declared
// in rpcproxy.h, so we need this declaration.
void RPC_ENTRY GetProxyDllInfo(const ProxyFileInfo*** aInfo, const CLSID** aId);
}

namespace mozilla {
namespace mscom {

static bool GetContainingLibPath(wchar_t* aBuffer, size_t aBufferLen) {
  HMODULE thisModule = reinterpret_cast<HMODULE>(GetContainingModuleHandle());
  if (!thisModule) {
    return false;
  }

  DWORD fileNameResult = GetModuleFileName(thisModule, aBuffer, aBufferLen);
  if (!fileNameResult || (fileNameResult == aBufferLen &&
                          ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    return false;
  }

  return true;
}

static bool BuildLibPath(RegistrationFlags aFlags, wchar_t* aBuffer,
                         size_t aBufferLen, const wchar_t* aLeafName) {
  if (aFlags == RegistrationFlags::eUseBinDirectory) {
    if (!GetContainingLibPath(aBuffer, aBufferLen)) {
      return false;
    }

    if (!PathRemoveFileSpec(aBuffer)) {
      return false;
    }
  } else if (aFlags == RegistrationFlags::eUseSystemDirectory) {
    UINT result = GetSystemDirectoryW(aBuffer, static_cast<UINT>(aBufferLen));
    if (!result || result > aBufferLen) {
      return false;
    }
  } else {
    return false;
  }

  if (!PathAppend(aBuffer, aLeafName)) {
    return false;
  }
  return true;
}

static bool RegisterPSClsids(const ProxyFileInfo** aProxyInfo,
                             const CLSID* aProxyClsid) {
  while (*aProxyInfo) {
    const ProxyFileInfo& curInfo = **aProxyInfo;
    for (unsigned short idx = 0, size = curInfo.TableSize; idx < size; ++idx) {
      HRESULT hr = CoRegisterPSClsid(*(curInfo.pStubVtblList[idx]->header.piid),
                                     *aProxyClsid);
      if (FAILED(hr)) {
        return false;
      }
    }
    ++aProxyInfo;
  }

  return true;
}

UniquePtr<RegisteredProxy> RegisterProxy() {
  const ProxyFileInfo** proxyInfo = nullptr;
  const CLSID* proxyClsid = nullptr;
  GetProxyDllInfo(&proxyInfo, &proxyClsid);
  if (!proxyInfo || !proxyClsid) {
    return nullptr;
  }

  IUnknown* classObject = nullptr;
  HRESULT hr =
      DllGetClassObject(*proxyClsid, IID_IUnknown, (void**)&classObject);
  if (FAILED(hr)) {
    return nullptr;
  }

  DWORD regCookie;
  hr = CoRegisterClassObject(*proxyClsid, classObject, CLSCTX_INPROC_SERVER,
                             REGCLS_MULTIPLEUSE, &regCookie);
  if (FAILED(hr)) {
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }

  wchar_t modulePathBuf[MAX_PATH + 1] = {0};
  if (!GetContainingLibPath(modulePathBuf, ArrayLength(modulePathBuf))) {
    CoRevokeClassObject(regCookie);
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }

  ITypeLib* typeLib = nullptr;
  hr = LoadTypeLibEx(modulePathBuf, REGKIND_NONE, &typeLib);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    CoRevokeClassObject(regCookie);
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }

#if defined(MOZILLA_INTERNAL_API)
  hr = RegisterPassthruProxy();
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    CoRevokeClassObject(regCookie);
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }
#endif  // defined(MOZILLA_INTERNAL_API)

  // RegisteredProxy takes ownership of classObject and typeLib references
  auto result(MakeUnique<RegisteredProxy>(classObject, regCookie, typeLib));

  if (!RegisterPSClsids(proxyInfo, proxyClsid)) {
    return nullptr;
  }

  return result;
}

UniquePtr<RegisteredProxy> RegisterProxy(const wchar_t* aLeafName,
                                         RegistrationFlags aFlags) {
  wchar_t modulePathBuf[MAX_PATH + 1] = {0};
  if (!BuildLibPath(aFlags, modulePathBuf, ArrayLength(modulePathBuf),
                    aLeafName)) {
    return nullptr;
  }

  nsModuleHandle proxyDll(LoadLibrary(modulePathBuf));
  if (!proxyDll.get()) {
    return nullptr;
  }

  // Instantiate an activation context so that CoGetClassObject will use any
  // COM metadata embedded in proxyDll's manifest to resolve CLSIDs.
  ActivationContextRegion actCtxRgn(proxyDll.get());

  auto GetProxyDllInfoFn = reinterpret_cast<decltype(&GetProxyDllInfo)>(
      GetProcAddress(proxyDll, "GetProxyDllInfo"));
  if (!GetProxyDllInfoFn) {
    return nullptr;
  }

  const ProxyFileInfo** proxyInfo = nullptr;
  const CLSID* proxyClsid = nullptr;
  GetProxyDllInfoFn(&proxyInfo, &proxyClsid);
  if (!proxyInfo || !proxyClsid) {
    return nullptr;
  }

  // We call CoGetClassObject instead of DllGetClassObject because it forces
  // the COM runtime to manage the lifetime of the DLL.
  IUnknown* classObject = nullptr;
  HRESULT hr = CoGetClassObject(*proxyClsid, CLSCTX_INPROC_SERVER, nullptr,
                                IID_IUnknown, (void**)&classObject);
  if (FAILED(hr)) {
    return nullptr;
  }

  DWORD regCookie;
  hr = CoRegisterClassObject(*proxyClsid, classObject, CLSCTX_INPROC_SERVER,
                             REGCLS_MULTIPLEUSE, &regCookie);
  if (FAILED(hr)) {
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }

  ITypeLib* typeLib = nullptr;
  hr = LoadTypeLibEx(modulePathBuf, REGKIND_NONE, &typeLib);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    CoRevokeClassObject(regCookie);
    classObject->lpVtbl->Release(classObject);
    return nullptr;
  }

  // RegisteredProxy takes ownership of proxyDll, classObject, and typeLib
  // references
  auto result(MakeUnique<RegisteredProxy>(
      reinterpret_cast<uintptr_t>(proxyDll.disown()), classObject, regCookie,
      typeLib));

  if (!RegisterPSClsids(proxyInfo, proxyClsid)) {
    return nullptr;
  }

  return result;
}

UniquePtr<RegisteredProxy> RegisterTypelib(const wchar_t* aLeafName,
                                           RegistrationFlags aFlags) {
  wchar_t modulePathBuf[MAX_PATH + 1] = {0};
  if (!BuildLibPath(aFlags, modulePathBuf, ArrayLength(modulePathBuf),
                    aLeafName)) {
    return nullptr;
  }

  ITypeLib* typeLib = nullptr;
  HRESULT hr = LoadTypeLibEx(modulePathBuf, REGKIND_NONE, &typeLib);
  if (FAILED(hr)) {
    return nullptr;
  }

  // RegisteredProxy takes ownership of typeLib reference
  auto result(MakeUnique<RegisteredProxy>(typeLib));
  return result;
}

RegisteredProxy::RegisteredProxy(uintptr_t aModule, IUnknown* aClassObject,
                                 uint32_t aRegCookie, ITypeLib* aTypeLib)
    : mModule(aModule),
      mClassObject(aClassObject),
      mRegCookie(aRegCookie),
      mTypeLib(aTypeLib)
#if defined(MOZILLA_INTERNAL_API)
      ,
      mIsRegisteredInMTA(IsCurrentThreadMTA())
#endif  // defined(MOZILLA_INTERNAL_API)
{
  MOZ_ASSERT(aClassObject);
  MOZ_ASSERT(aTypeLib);
  AddToRegistry(this);
}

RegisteredProxy::RegisteredProxy(IUnknown* aClassObject, uint32_t aRegCookie,
                                 ITypeLib* aTypeLib)
    : mModule(0),
      mClassObject(aClassObject),
      mRegCookie(aRegCookie),
      mTypeLib(aTypeLib)
#if defined(MOZILLA_INTERNAL_API)
      ,
      mIsRegisteredInMTA(IsCurrentThreadMTA())
#endif  // defined(MOZILLA_INTERNAL_API)
{
  MOZ_ASSERT(aClassObject);
  MOZ_ASSERT(aTypeLib);
  AddToRegistry(this);
}

// If we're initializing from a typelib, it doesn't matter which apartment we
// run in, so mIsRegisteredInMTA may always be set to false in this case.
RegisteredProxy::RegisteredProxy(ITypeLib* aTypeLib)
    : mModule(0),
      mClassObject(nullptr),
      mRegCookie(0),
      mTypeLib(aTypeLib)
#if defined(MOZILLA_INTERNAL_API)
      ,
      mIsRegisteredInMTA(false)
#endif  // defined(MOZILLA_INTERNAL_API)
{
  MOZ_ASSERT(aTypeLib);
  AddToRegistry(this);
}

void RegisteredProxy::Clear() {
  if (mTypeLib) {
    mTypeLib->lpVtbl->Release(mTypeLib);
    mTypeLib = nullptr;
  }
  if (mClassObject) {
    // NB: mClassObject and mRegCookie must be freed from inside the apartment
    // which they were created in.
    auto cleanupFn = [&]() -> void {
      ::CoRevokeClassObject(mRegCookie);
      mRegCookie = 0;
      mClassObject->lpVtbl->Release(mClassObject);
      mClassObject = nullptr;
    };
#if defined(MOZILLA_INTERNAL_API)
    // This code only supports MTA when built internally
    if (mIsRegisteredInMTA) {
      EnsureMTA mta(cleanupFn);
    } else {
      cleanupFn();
    }
#else
    cleanupFn();
#endif  // defined(MOZILLA_INTERNAL_API)
  }
  if (mModule) {
    ::FreeLibrary(reinterpret_cast<HMODULE>(mModule));
    mModule = 0;
  }
}

RegisteredProxy::~RegisteredProxy() {
  DeleteFromRegistry(this);
  Clear();
}

RegisteredProxy::RegisteredProxy(RegisteredProxy&& aOther)
    : mModule(0),
      mClassObject(nullptr),
      mRegCookie(0),
      mTypeLib(nullptr)
#if defined(MOZILLA_INTERNAL_API)
      ,
      mIsRegisteredInMTA(false)
#endif  // defined(MOZILLA_INTERNAL_API)
{
  *this = std::forward<RegisteredProxy>(aOther);
  AddToRegistry(this);
}

RegisteredProxy& RegisteredProxy::operator=(RegisteredProxy&& aOther) {
  Clear();

  mModule = aOther.mModule;
  aOther.mModule = 0;
  mClassObject = aOther.mClassObject;
  aOther.mClassObject = nullptr;
  mRegCookie = aOther.mRegCookie;
  aOther.mRegCookie = 0;
  mTypeLib = aOther.mTypeLib;
  aOther.mTypeLib = nullptr;

#if defined(MOZILLA_INTERNAL_API)
  mIsRegisteredInMTA = aOther.mIsRegisteredInMTA;
#endif  // defined(MOZILLA_INTERNAL_API)

  return *this;
}

HRESULT
RegisteredProxy::GetTypeInfoForGuid(REFGUID aGuid,
                                    ITypeInfo** aOutTypeInfo) const {
  if (!aOutTypeInfo) {
    return E_INVALIDARG;
  }
  if (!mTypeLib) {
    return E_UNEXPECTED;
  }
  return mTypeLib->lpVtbl->GetTypeInfoOfGuid(mTypeLib, aGuid, aOutTypeInfo);
}

static StaticAutoPtr<Vector<RegisteredProxy*>> sRegistry;

namespace UseGetMutexForAccess {

// This must not be accessed directly; use GetMutex() instead
static CRITICAL_SECTION sMutex;

}  // namespace UseGetMutexForAccess

static CRITICAL_SECTION* GetMutex() {
  static CRITICAL_SECTION& mutex = []() -> CRITICAL_SECTION& {
#if defined(RELEASE_OR_BETA)
    DWORD flags = CRITICAL_SECTION_NO_DEBUG_INFO;
#else
    DWORD flags = 0;
#endif
    InitializeCriticalSectionEx(&UseGetMutexForAccess::sMutex, 4000, flags);
#if !defined(MOZILLA_INTERNAL_API)
    atexit([]() { DeleteCriticalSection(&UseGetMutexForAccess::sMutex); });
#endif
    return UseGetMutexForAccess::sMutex;
  }();
  return &mutex;
}

/* static */
bool RegisteredProxy::Find(REFIID aIid, ITypeInfo** aTypeInfo) {
  AutoCriticalSection lock(GetMutex());

  if (!sRegistry) {
    return false;
  }

  for (auto&& proxy : *sRegistry) {
    if (SUCCEEDED(proxy->GetTypeInfoForGuid(aIid, aTypeInfo))) {
      return true;
    }
  }

  return false;
}

/* static */
void RegisteredProxy::AddToRegistry(RegisteredProxy* aProxy) {
  MOZ_ASSERT(aProxy);

  AutoCriticalSection lock(GetMutex());

  if (!sRegistry) {
    sRegistry = new Vector<RegisteredProxy*>();

#if !defined(MOZILLA_INTERNAL_API)
    // sRegistry allocation is fallible outside of Mozilla processes
    if (!sRegistry) {
      return;
    }
#endif
  }

  MOZ_ALWAYS_TRUE(sRegistry->emplaceBack(aProxy));
}

/* static */
void RegisteredProxy::DeleteFromRegistry(RegisteredProxy* aProxy) {
  MOZ_ASSERT(aProxy);

  AutoCriticalSection lock(GetMutex());

  MOZ_ASSERT(sRegistry && !sRegistry->empty());

  if (!sRegistry) {
    return;
  }

  sRegistry->erase(std::remove(sRegistry->begin(), sRegistry->end(), aProxy),
                   sRegistry->end());

  if (sRegistry->empty()) {
    sRegistry = nullptr;
  }
}

#if defined(MOZILLA_INTERNAL_API)

static StaticAutoPtr<Vector<Pair<const ArrayData*, size_t>>> sArrayData;

void RegisterArrayData(const ArrayData* aArrayData, size_t aLength) {
  AutoCriticalSection lock(GetMutex());

  if (!sArrayData) {
    sArrayData = new Vector<Pair<const ArrayData*, size_t>>();
    ClearOnShutdown(&sArrayData, ShutdownPhase::ShutdownThreads);
  }

  MOZ_ALWAYS_TRUE(sArrayData->emplaceBack(MakePair(aArrayData, aLength)));
}

const ArrayData* FindArrayData(REFIID aIid, ULONG aMethodIndex) {
  AutoCriticalSection lock(GetMutex());

  if (!sArrayData) {
    return nullptr;
  }

  for (auto&& data : *sArrayData) {
    for (size_t innerIdx = 0, innerLen = data.second(); innerIdx < innerLen;
         ++innerIdx) {
      const ArrayData* array = data.first();
      if (aMethodIndex == array[innerIdx].mMethodIndex &&
          IsInterfaceEqualToOrInheritedFrom(aIid, array[innerIdx].mIid,
                                            aMethodIndex)) {
        return &array[innerIdx];
      }
    }
  }

  return nullptr;
}

#endif  // defined(MOZILLA_INTERNAL_API)

}  // namespace mscom
}  // namespace mozilla

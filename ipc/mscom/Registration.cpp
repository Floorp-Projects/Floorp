/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// COM registration data structures are built with C code, so we need to
// simulate that in our C++ code by defining CINTERFACE before including
// anything else that could possibly pull in Windows header files.
#define CINTERFACE

#include "mozilla/mscom/Registration.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Move.h"
#include "mozilla/Mutex.h"
#include "mozilla/Pair.h"
#include "mozilla/StaticPtr.h"
#include "nsTArray.h"

#include <oaidl.h>
#include <objidl.h>
#include <rpcproxy.h>
#include <shlwapi.h>

/* This code MUST NOT use any non-inlined internal Mozilla APIs, as it will be
   compiled into DLLs that COM may load into non-Mozilla processes! */

namespace {

// This function is defined in generated code for proxy DLLs but is not declared
// in rpcproxy.h, so we need this typedef.
typedef void (RPC_ENTRY *GetProxyDllInfoFnPtr)(const ProxyFileInfo*** aInfo,
                                               const CLSID** aId);

} // anonymous namespace

namespace mozilla {
namespace mscom {

static bool
BuildLibPath(RegistrationFlags aFlags, wchar_t* aBuffer, size_t aBufferLen,
             const wchar_t* aLeafName)
{
  if (aFlags == RegistrationFlags::eUseBinDirectory) {
    HMODULE thisModule = nullptr;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCTSTR>(&RegisterProxy),
                           &thisModule)) {
      return false;
    }
    DWORD fileNameResult = GetModuleFileName(thisModule, aBuffer, aBufferLen);
    if (!fileNameResult || (fileNameResult == aBufferLen &&
          ::GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
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

UniquePtr<RegisteredProxy>
RegisterProxy(const wchar_t* aLeafName, RegistrationFlags aFlags)
{
  wchar_t modulePathBuf[MAX_PATH + 1] = {0};
  if (!BuildLibPath(aFlags, modulePathBuf, ArrayLength(modulePathBuf),
                    aLeafName)) {
    return nullptr;
  }

  HMODULE proxyDll = LoadLibrary(modulePathBuf);
  if (!proxyDll) {
    return nullptr;
  }

  auto DllGetClassObjectFn = reinterpret_cast<LPFNGETCLASSOBJECT>(
      GetProcAddress(proxyDll, "DllGetClassObject"));
  if (!DllGetClassObjectFn) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  auto GetProxyDllInfoFn = reinterpret_cast<GetProxyDllInfoFnPtr>(
      GetProcAddress(proxyDll, "GetProxyDllInfo"));
  if (!GetProxyDllInfoFn) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  const ProxyFileInfo** proxyInfo = nullptr;
  const CLSID* proxyClsid = nullptr;
  GetProxyDllInfoFn(&proxyInfo, &proxyClsid);
  if (!proxyInfo || !proxyClsid) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  IUnknown* classObject = nullptr;
  HRESULT hr = DllGetClassObjectFn(*proxyClsid, IID_IUnknown,
                                   (void**) &classObject);
  if (FAILED(hr)) {
    FreeLibrary(proxyDll);
    return nullptr;
  }

  DWORD regCookie;
  hr = CoRegisterClassObject(*proxyClsid, classObject, CLSCTX_INPROC_SERVER,
                             REGCLS_MULTIPLEUSE, &regCookie);
  if (FAILED(hr)) {
    classObject->lpVtbl->Release(classObject);
    FreeLibrary(proxyDll);
    return nullptr;
  }

  ITypeLib* typeLib = nullptr;
  hr = LoadTypeLibEx(modulePathBuf, REGKIND_NONE, &typeLib);
  MOZ_ASSERT(SUCCEEDED(hr));
  if (FAILED(hr)) {
    CoRevokeClassObject(regCookie);
    classObject->lpVtbl->Release(classObject);
    FreeLibrary(proxyDll);
    return nullptr;
  }

  // RegisteredProxy takes ownership of proxyDll, classObject, and typeLib
  // references
  auto result(MakeUnique<RegisteredProxy>(reinterpret_cast<uintptr_t>(proxyDll),
                                          classObject, regCookie, typeLib));

  while (*proxyInfo) {
    const ProxyFileInfo& curInfo = **proxyInfo;
    for (unsigned short i = 0, e = curInfo.TableSize; i < e; ++i) {
      hr = CoRegisterPSClsid(*(curInfo.pStubVtblList[i]->header.piid),
                             *proxyClsid);
      if (FAILED(hr)) {
        return nullptr;
      }
    }
    ++proxyInfo;
  }

  return result;
}

UniquePtr<RegisteredProxy>
RegisterTypelib(const wchar_t* aLeafName, RegistrationFlags aFlags)
{
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
  : mModule(aModule)
  , mClassObject(aClassObject)
  , mRegCookie(aRegCookie)
  , mTypeLib(aTypeLib)
{
  MOZ_ASSERT(aClassObject);
  MOZ_ASSERT(aTypeLib);
  AddToRegistry(this);
}

RegisteredProxy::RegisteredProxy(ITypeLib* aTypeLib)
  : mModule(0)
  , mClassObject(nullptr)
  , mRegCookie(0)
  , mTypeLib(aTypeLib)
{
  MOZ_ASSERT(aTypeLib);
  AddToRegistry(this);
}

RegisteredProxy::~RegisteredProxy()
{
  DeleteFromRegistry(this);
  if (mTypeLib) {
    mTypeLib->lpVtbl->Release(mTypeLib);
  }
  if (mClassObject) {
    ::CoRevokeClassObject(mRegCookie);
    mClassObject->lpVtbl->Release(mClassObject);
  }
  if (mModule) {
    ::FreeLibrary(reinterpret_cast<HMODULE>(mModule));
  }
}

RegisteredProxy::RegisteredProxy(RegisteredProxy&& aOther)
{
  *this = mozilla::Forward<RegisteredProxy>(aOther);
}

RegisteredProxy&
RegisteredProxy::operator=(RegisteredProxy&& aOther)
{
  mModule = aOther.mModule;
  aOther.mModule = 0;
  mClassObject = aOther.mClassObject;
  aOther.mClassObject = nullptr;
  mRegCookie = aOther.mRegCookie;
  aOther.mRegCookie = 0;
  mTypeLib = aOther.mTypeLib;
  aOther.mTypeLib = nullptr;
  return *this;
}

HRESULT
RegisteredProxy::GetTypeInfoForInterface(REFIID aIid,
                                         ITypeInfo** aOutTypeInfo) const
{
  if (!aOutTypeInfo) {
    return E_INVALIDARG;
  }
  if (!mTypeLib) {
    return E_UNEXPECTED;
  }
  return mTypeLib->lpVtbl->GetTypeInfoOfGuid(mTypeLib, aIid, aOutTypeInfo);
}

static StaticAutoPtr<nsTArray<RegisteredProxy*>> sRegistry;
static StaticAutoPtr<Mutex> sRegMutex;
static StaticAutoPtr<nsTArray<Pair<const ArrayData*, size_t>>> sArrayData;

static Mutex&
GetMutex()
{
  static Mutex& mutex = []() -> Mutex& {
    if (!sRegMutex) {
      sRegMutex = new Mutex("RegisteredProxy::sRegMutex");
      ClearOnShutdown(&sRegMutex, ShutdownPhase::ShutdownThreads);
    }
    return *sRegMutex;
  }();
  return mutex;
}

/* static */ bool
RegisteredProxy::Find(REFIID aIid, ITypeInfo** aTypeInfo)
{
  MutexAutoLock lock(GetMutex());
  nsTArray<RegisteredProxy*>& registry = *sRegistry;
  for (uint32_t idx = 0, len = registry.Length(); idx < len; ++idx) {
    if (SUCCEEDED(registry[idx]->GetTypeInfoForInterface(aIid, aTypeInfo))) {
      return true;
    }
  }
  return false;
}

/* static */ void
RegisteredProxy::AddToRegistry(RegisteredProxy* aProxy)
{
  MutexAutoLock lock(GetMutex());
  if (!sRegistry) {
    sRegistry = new nsTArray<RegisteredProxy*>();
    ClearOnShutdown(&sRegistry);
  }
  sRegistry->AppendElement(aProxy);
}

/* static */ void
RegisteredProxy::DeleteFromRegistry(RegisteredProxy* aProxy)
{
  MutexAutoLock lock(GetMutex());
  sRegistry->RemoveElement(aProxy);
}

void
RegisterArrayData(const ArrayData* aArrayData, size_t aLength)
{
  MutexAutoLock lock(GetMutex());
  if (!sArrayData) {
    sArrayData = new nsTArray<Pair<const ArrayData*, size_t>>();
    ClearOnShutdown(&sArrayData, ShutdownPhase::ShutdownThreads);
  }
  sArrayData->AppendElement(MakePair(aArrayData, aLength));
}

const ArrayData*
FindArrayData(REFIID aIid, ULONG aMethodIndex)
{
  MutexAutoLock lock(GetMutex());
  if (!sArrayData) {
    return nullptr;
  }
  for (uint32_t outerIdx = 0, outerLen = sArrayData->Length();
       outerIdx < outerLen; ++outerIdx) {
    auto& data = sArrayData->ElementAt(outerIdx);
    for (size_t innerIdx = 0, innerLen = data.second(); innerIdx < innerLen;
         ++innerIdx) {
      const ArrayData* array = data.first();
      if (aIid == array[innerIdx].mIid &&
          aMethodIndex == array[innerIdx].mMethodIndex) {
        return &array[innerIdx];
      }
    }
  }
  return nullptr;
}

} // namespace mscom
} // namespace mozilla

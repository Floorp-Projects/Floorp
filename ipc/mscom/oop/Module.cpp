/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Module.h"

#include <stdlib.h>

#include <ktmw32.h>
#include <memory.h>
#include <rpc.h>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/Range.h"
#include "nsWindowsHelpers.h"

template <size_t N>
static const mozilla::Range<const wchar_t> LiteralToRange(
    const wchar_t (&aArg)[N]) {
  return mozilla::Range(aArg, N);
}

namespace mozilla {
namespace mscom {

ULONG Module::sRefCount = 0;

static const wchar_t* SubkeyNameFromClassType(
    const Module::ClassType aClassType) {
  switch (aClassType) {
    case Module::ClassType::InprocServer:
      return L"InprocServer32";
    case Module::ClassType::InprocHandler:
      return L"InprocHandler32";
    default:
      MOZ_CRASH("Unknown ClassType");
      return nullptr;
  }
}

static const Range<const wchar_t> ThreadingModelAsString(
    const Module::ThreadingModel aThreadingModel) {
  switch (aThreadingModel) {
    case Module::ThreadingModel::DedicatedUiThreadOnly:
      return LiteralToRange(L"Apartment");
    case Module::ThreadingModel::MultiThreadedApartmentOnly:
      return LiteralToRange(L"Free");
    case Module::ThreadingModel::DedicatedUiThreadXorMultiThreadedApartment:
      return LiteralToRange(L"Both");
    case Module::ThreadingModel::AllThreadsAllApartments:
      return LiteralToRange(L"Neutral");
    default:
      MOZ_CRASH("Unknown ThreadingModel");
      return Range<const wchar_t>();
  }
}

/* static */
HRESULT Module::Register(const CLSID* const* aClsids, const size_t aNumClsids,
                         const ThreadingModel aThreadingModel,
                         const ClassType aClassType, const GUID* const aAppId) {
  MOZ_ASSERT(aClsids && aNumClsids);
  if (!aClsids || !aNumClsids) {
    return E_INVALIDARG;
  }

  const wchar_t* inprocName = SubkeyNameFromClassType(aClassType);

  const Range<const wchar_t> threadingModelStr =
      ThreadingModelAsString(aThreadingModel);
  const DWORD threadingModelStrLenBytesInclNul =
      threadingModelStr.length() * sizeof(wchar_t);

  wchar_t strAppId[kGuidRegFormatCharLenInclNul] = {};
  if (aAppId) {
    GUIDToString(*aAppId, strAppId);
  }

  // Obtain the full path to this DLL
  HMODULE thisModule;
  if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&Module::CanUnload),
                            &thisModule)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  wchar_t absThisModulePath[MAX_PATH + 1] = {};
  DWORD actualPathLenCharsExclNul = ::GetModuleFileNameW(
      thisModule, absThisModulePath, ArrayLength(absThisModulePath));
  if (!actualPathLenCharsExclNul ||
      actualPathLenCharsExclNul == ArrayLength(absThisModulePath)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }
  const DWORD actualPathLenBytesInclNul =
      (actualPathLenCharsExclNul + 1) * sizeof(wchar_t);

  // Use the name of this DLL as the name of the transaction
  wchar_t txnName[_MAX_FNAME] = {};
  if (_wsplitpath_s(absThisModulePath, nullptr, 0, nullptr, 0, txnName,
                    ArrayLength(txnName), nullptr, 0)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  // Manipulate the registry using a transaction so that any failures are
  // rolled back.
  nsAutoHandle txn(::CreateTransaction(
      nullptr, nullptr, TRANSACTION_DO_NOT_PROMOTE, 0, 0, 0, txnName));
  if (txn.get() == INVALID_HANDLE_VALUE) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  HRESULT hr;
  LSTATUS status;

  // A single DLL may serve multiple components. For each CLSID, we register
  // this DLL as its server and, when an AppId is specified, set up a reference
  // from the CLSID to the specified AppId.
  for (size_t idx = 0; idx < aNumClsids; ++idx) {
    if (!aClsids[idx]) {
      return E_INVALIDARG;
    }

    wchar_t clsidKeyPath[256];
    hr = BuildClsidPath(*aClsids[idx], clsidKeyPath);
    if (FAILED(hr)) {
      return hr;
    }

    // Create the CLSID key
    HKEY rawClsidKey;
    status = ::RegCreateKeyTransactedW(
        HKEY_LOCAL_MACHINE, clsidKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, nullptr, &rawClsidKey, nullptr, txn, nullptr);
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }
    nsAutoRegKey clsidKey(rawClsidKey);

    if (aAppId) {
      // This value associates the registered CLSID with the specified AppID
      status = ::RegSetValueExW(clsidKey, L"AppID", 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(strAppId),
                                ArrayLength(strAppId) * sizeof(wchar_t));
      if (status != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(status);
      }
    }

    HKEY rawInprocKey;
    status = ::RegCreateKeyTransactedW(
        clsidKey, inprocName, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, nullptr, &rawInprocKey, nullptr, txn, nullptr);
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }
    nsAutoRegKey inprocKey(rawInprocKey);

    // Set the component's path to this DLL
    status = ::RegSetValueExW(inprocKey, nullptr, 0, REG_EXPAND_SZ,
                              reinterpret_cast<const BYTE*>(absThisModulePath),
                              actualPathLenBytesInclNul);
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }

    status = ::RegSetValueExW(
        inprocKey, L"ThreadingModel", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(threadingModelStr.begin().get()),
        threadingModelStrLenBytesInclNul);
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }
  }

  if (aAppId) {
    // When specified, we must also create a key for the AppID.
    wchar_t appidKeyPath[256];
    hr = BuildAppidPath(*aAppId, appidKeyPath);
    if (FAILED(hr)) {
      return hr;
    }

    HKEY rawAppidKey;
    status = ::RegCreateKeyTransactedW(
        HKEY_LOCAL_MACHINE, appidKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS, nullptr, &rawAppidKey, nullptr, txn, nullptr);
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }
    nsAutoRegKey appidKey(rawAppidKey);

    // Setting DllSurrogate to a null or empty string indicates to Windows that
    // we want to use the default surrogate (i.e. dllhost.exe) to load our DLL.
    status =
        ::RegSetValueExW(appidKey, L"DllSurrogate", 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
    if (status != ERROR_SUCCESS) {
      return HRESULT_FROM_WIN32(status);
    }
  }

  if (!::CommitTransaction(txn)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

/**
 * Unfortunately the registry transaction APIs are not as well-developed for
 * deleting things as they are for creating them. We just use RegDeleteTree
 * for the implementation of this method.
 */
HRESULT Module::Deregister(const CLSID* const* aClsids, const size_t aNumClsids,
                           const GUID* const aAppId) {
  MOZ_ASSERT(aClsids && aNumClsids);
  if (!aClsids || !aNumClsids) {
    return E_INVALIDARG;
  }

  HRESULT hr;
  LSTATUS status;

  // Delete the key for each CLSID. This will also delete any references to
  // the AppId.
  for (size_t idx = 0; idx < aNumClsids; ++idx) {
    if (!aClsids[idx]) {
      return E_INVALIDARG;
    }

    wchar_t clsidKeyPath[256];
    hr = BuildClsidPath(*aClsids[idx], clsidKeyPath);
    if (FAILED(hr)) {
      return hr;
    }

    status = ::RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKeyPath);
    // We allow the deletion to succeed if the key was already gone
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
      return HRESULT_FROM_WIN32(status);
    }
  }

  // Now delete the AppID key, if desired.
  if (aAppId) {
    wchar_t appidKeyPath[256];
    hr = BuildAppidPath(*aAppId, appidKeyPath);
    if (FAILED(hr)) {
      return hr;
    }

    status = ::RegDeleteTreeW(HKEY_LOCAL_MACHINE, appidKeyPath);
    // We allow the deletion to succeed if the key was already gone
    if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) {
      return HRESULT_FROM_WIN32(status);
    }
  }

  return S_OK;
}

}  // namespace mscom
}  // namespace mozilla

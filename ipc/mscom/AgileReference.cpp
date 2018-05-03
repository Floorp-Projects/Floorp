/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/AgileReference.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/Assertions.h"
#include "mozilla/Move.h"

#if NTDDI_VERSION < NTDDI_WINBLUE

// Declarations from Windows SDK specific to Windows 8.1

enum AgileReferenceOptions
{
    AGILEREFERENCE_DEFAULT        = 0,
    AGILEREFERENCE_DELAYEDMARSHAL = 1,
};

HRESULT WINAPI RoGetAgileReference(AgileReferenceOptions options,
                                   REFIID riid, IUnknown* pUnk,
                                   IAgileReference** ppAgileReference);

#endif // NTDDI_VERSION < NTDDI_WINBLUE

namespace mozilla {
namespace mscom {

AgileReference::AgileReference(REFIID aIid, IUnknown* aObject)
  : mIid(aIid)
  , mGitCookie(0)
{
  /*
   * There are two possible techniques for creating agile references. Starting
   * with Windows 8.1, we may use the RoGetAgileReference API, which is faster.
   * If that API is not available, we fall back to using the Global Interface
   * Table.
   */
  static const DynamicallyLinkedFunctionPtr<decltype(&::RoGetAgileReference)>
    pRoGetAgileReference(L"ole32.dll", "RoGetAgileReference");

  MOZ_ASSERT(aObject);

  if (pRoGetAgileReference &&
      SUCCEEDED(pRoGetAgileReference(AGILEREFERENCE_DEFAULT, aIid, aObject,
                                     getter_AddRefs(mAgileRef)))) {
    return;
  }

  IGlobalInterfaceTable* git = ObtainGit();
  MOZ_ASSERT(git);
  if (!git) {
    return;
  }

  DebugOnly<HRESULT> hr = git->RegisterInterfaceInGlobal(aObject, aIid,
                                                         &mGitCookie);
  MOZ_ASSERT(SUCCEEDED(hr));
}

AgileReference::AgileReference(AgileReference&& aOther)
  : mIid(aOther.mIid)
  , mAgileRef(Move(aOther.mAgileRef))
  , mGitCookie(aOther.mGitCookie)
{
  aOther.mGitCookie = 0;
}

AgileReference::~AgileReference()
{
  if (!mGitCookie) {
    return;
  }

  IGlobalInterfaceTable* git = ObtainGit();
  MOZ_ASSERT(git);
  if (!git) {
    return;
  }

  DebugOnly<HRESULT> hr = git->RevokeInterfaceFromGlobal(mGitCookie);
  MOZ_ASSERT(SUCCEEDED(hr));
}

HRESULT
AgileReference::Resolve(REFIID aIid, void** aOutInterface)
{
  MOZ_ASSERT(aOutInterface);
  MOZ_ASSERT(mAgileRef || mGitCookie);

  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  if (mAgileRef) {
    // IAgileReference lets you directly resolve the interface you want...
    return mAgileRef->Resolve(aIid, aOutInterface);
  }

  if (!mGitCookie) {
    return E_UNEXPECTED;
  }

  IGlobalInterfaceTable* git = ObtainGit();
  MOZ_ASSERT(git);
  if (!git) {
    return E_UNEXPECTED;
  }

  RefPtr<IUnknown> originalInterface;
  HRESULT hr = git->GetInterfaceFromGlobal(mGitCookie, mIid,
                                           getter_AddRefs(originalInterface));
  if (FAILED(hr)) {
    return hr;
  }

  if (aIid == mIid) {
    originalInterface.forget(aOutInterface);
    return S_OK;
  }

  // ...Whereas the GIT requires us to obtain the same interface that we
  // requested and then QI for the desired interface afterward.
  return originalInterface->QueryInterface(aIid, aOutInterface);
}

IGlobalInterfaceTable*
AgileReference::ObtainGit()
{
  // Internally to COM, the Global Interface Table is a singleton, therefore we
  // don't worry about holding onto this reference indefinitely.
  static IGlobalInterfaceTable* sGit = []() -> IGlobalInterfaceTable* {
    IGlobalInterfaceTable* result = nullptr;
    DebugOnly<HRESULT> hr =
      ::CoCreateInstance(CLSID_StdGlobalInterfaceTable, nullptr,
                         CLSCTX_INPROC_SERVER, IID_IGlobalInterfaceTable,
                         reinterpret_cast<void**>(&result));
    MOZ_ASSERT(SUCCEEDED(hr));
    return result;
  }();

  return sGit;
}

} // namespace mscom
} // namespace mozilla

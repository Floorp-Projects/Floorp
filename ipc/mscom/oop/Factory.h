/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Factory_h
#define mozilla_mscom_Factory_h

#if defined(MOZILLA_INTERNAL_API)
#error This code is NOT for internal Gecko use!
#endif // defined(MOZILLA_INTERNAL_API)

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StaticPtr.h"
#include "Module.h"

#include <objbase.h>
#include <unknwn.h>

/* WARNING! The code in this file may be loaded into the address spaces of other
   processes! It MUST NOT link against xul.dll or other Gecko binaries! Only
   inline code may be included! */

namespace mozilla {
namespace mscom {

template <typename T>
class MOZ_NONHEAP_CLASS Factory : public IClassFactory
{
  template <typename... Args>
  HRESULT DoCreate(Args... args)
  {
    MOZ_DIAGNOSTIC_ASSERT(false, "This should not be executed");
    return E_NOTIMPL;
  }

  template <typename... Args>
  HRESULT DoCreate(HRESULT (*aFnPtr)(IUnknown*, REFIID, void**), Args... args)
  {
    return aFnPtr(std::forward<Args>(args)...);
  }

public:
  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override
  {
    if (!aOutInterface) {
      return E_INVALIDARG;
    }

    if (aIid == IID_IUnknown || aIid == IID_IClassFactory) {
      RefPtr<IClassFactory> punk(this);
      punk.forget(aOutInterface);
      return S_OK;
    }

    *aOutInterface = nullptr;

    return E_NOINTERFACE;
  }

  STDMETHODIMP_(ULONG) AddRef() override
  {
    Module::Lock();
    return 2;
  }

  STDMETHODIMP_(ULONG) Release() override
  {
    Module::Unlock();
    return 1;
  }

  // IClassFactory
  STDMETHODIMP CreateInstance(IUnknown* aOuter, REFIID aIid,
                              void** aOutInterface) override
  {
    return DoCreate(&T::Create, aOuter, aIid, aOutInterface);
  }

  STDMETHODIMP LockServer(BOOL aLock) override
  {
    if (aLock) {
      Module::Lock();
    } else {
      Module::Unlock();
    }
    return S_OK;
  }
};

template <typename T>
class MOZ_NONHEAP_CLASS SingletonFactory : public Factory<T>
{
public:
  STDMETHODIMP CreateInstance(IUnknown* aOuter, REFIID aIid,
                              void** aOutInterface) override
  {
    if (aOuter || !aOutInterface) {
      return E_INVALIDARG;
    }

    RefPtr<T> obj(sInstance);
    if (!obj) {
      obj = GetOrCreateSingleton();
    }

    return obj->QueryInterface(aIid, aOutInterface);
  }

  RefPtr<T> GetOrCreateSingleton()
  {
    if (!sInstance) {
      RefPtr<T> object;
      if (FAILED(T::Create(getter_AddRefs(object)))) {
        return nullptr;
      }

      sInstance = object.forget();
    }

    return sInstance;
  }

  RefPtr<T> GetSingleton()
  {
    return sInstance;
  }

  void ClearSingleton()
  {
    if (!sInstance) {
      return;
    }

    DebugOnly<HRESULT> hr = ::CoDisconnectObject(sInstance.get(), 0);
    MOZ_ASSERT(SUCCEEDED(hr));
    sInstance = nullptr;
  }

private:
  static StaticRefPtr<T> sInstance;
};

template <typename T>
StaticRefPtr<T> SingletonFactory<T>::sInstance;

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Factory_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Module_h
#define mozilla_mscom_Module_h

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#include <objbase.h>

/* WARNING! The code in this file may be loaded into the address spaces of other
   processes! It MUST NOT link against xul.dll or other Gecko binaries! Only
   inline code may be included! */

namespace mozilla {
namespace mscom {

class Module {
 public:
  static HRESULT CanUnload() { return sRefCount == 0 ? S_OK : S_FALSE; }

  static void Lock() { ++sRefCount; }
  static void Unlock() { --sRefCount; }

  enum class ThreadingModel {
    DedicatedUiThreadOnly,
    MultiThreadedApartmentOnly,
    DedicatedUiThreadXorMultiThreadedApartment,
    AllThreadsAllApartments,
  };

  enum class ClassType {
    InprocServer,
    InprocHandler,
  };

  static HRESULT Register(REFCLSID aClsid, const ThreadingModel aThreadingModel,
                          const ClassType aClassType = ClassType::InprocServer,
                          const GUID* const aAppId = nullptr) {
    const CLSID* clsidArray[] = {&aClsid};
    return Register(clsidArray, aThreadingModel, aClassType, aAppId);
  }

  template <size_t N>
  static HRESULT Register(const CLSID* (&aClsids)[N],
                          const ThreadingModel aThreadingModel,
                          const ClassType aClassType = ClassType::InprocServer,
                          const GUID* const aAppId = nullptr) {
    return Register(aClsids, N, aThreadingModel, aClassType, aAppId);
  }

  static HRESULT Deregister(REFCLSID aClsid,
                            const GUID* const aAppId = nullptr) {
    const CLSID* clsidArray[] = {&aClsid};
    return Deregister(clsidArray, aAppId);
  }

  template <size_t N>
  static HRESULT Deregister(const CLSID* (&aClsids)[N],
                            const GUID* const aAppId = nullptr) {
    return Deregister(aClsids, N, aAppId);
  }

 private:
  static HRESULT Register(const CLSID* const* aClsids, const size_t aNumClsids,
                          const ThreadingModel aThreadingModel,
                          const ClassType aClassType, const GUID* const aAppId);

  static HRESULT Deregister(const CLSID* const* aClsids,
                            const size_t aNumClsids, const GUID* const aAppId);

 private:
  static ULONG sRefCount;
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_Module_h

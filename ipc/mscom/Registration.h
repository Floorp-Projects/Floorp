/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_Registration_h
#define mozilla_mscom_Registration_h

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

#include <objbase.h>

struct ITypeInfo;
struct ITypeLib;

namespace mozilla {
namespace mscom {

/**
 * Assumptions:
 * (1) The DLL exports GetProxyDllInfo. This is not exported by default; it must
 *     be specified in the EXPORTS section of the DLL's module definition file.
 */
class RegisteredProxy
{
public:
  RegisteredProxy(uintptr_t aModule, IUnknown* aClassObject,
                  uint32_t aRegCookie, ITypeLib* aTypeLib);
  explicit RegisteredProxy(ITypeLib* aTypeLib);
  RegisteredProxy(RegisteredProxy&& aOther);
  RegisteredProxy& operator=(RegisteredProxy&& aOther);

  ~RegisteredProxy();

  HRESULT GetTypeInfoForInterface(REFIID aIid, ITypeInfo** aOutTypeInfo) const;

  static bool Find(REFIID aIid, ITypeInfo** aOutTypeInfo);

private:
  RegisteredProxy() = delete;
  RegisteredProxy(RegisteredProxy&) = delete;
  RegisteredProxy& operator=(RegisteredProxy&) = delete;

  static void AddToRegistry(RegisteredProxy* aProxy);
  static void DeleteFromRegistry(RegisteredProxy* aProxy);

private:
  // Not using Windows types here: We shouldn't #include windows.h
  // since it might pull in COM code which we want to do very carefully in
  // Registration.cpp.
  uintptr_t mModule;
  IUnknown* mClassObject;
  uint32_t  mRegCookie;
  ITypeLib* mTypeLib;
};

enum class RegistrationFlags
{
  eUseBinDirectory,
  eUseSystemDirectory
};

// For DLL files. Assumes corresponding TLB is embedded in resources.
UniquePtr<RegisteredProxy> RegisterProxy(const wchar_t* aLeafName,
                                         RegistrationFlags aFlags =
                                           RegistrationFlags::eUseBinDirectory);
// For standalone TLB files.
UniquePtr<RegisteredProxy> RegisterTypelib(const wchar_t* aLeafName,
                                           RegistrationFlags aFlags =
                                             RegistrationFlags::eUseBinDirectory);

/**
 * The COM interceptor uses type library information to build its interface
 * proxies. Unfortunately type libraries do not encode size_is and length_is
 * annotations that have been specified in IDL. This structure allows us to
 * explicitly declare such relationships so that the COM interceptor may
 * be made aware of them.
 */
struct ArrayData
{
  ArrayData(REFIID aIid, ULONG aMethodIndex, ULONG aArrayParamIndex,
            VARTYPE aArrayParamType, REFIID aArrayParamIid,
            ULONG aLengthParamIndex)
    : mIid(aIid)
    , mMethodIndex(aMethodIndex)
    , mArrayParamIndex(aArrayParamIndex)
    , mArrayParamType(aArrayParamType)
    , mArrayParamIid(aArrayParamIid)
    , mLengthParamIndex(aLengthParamIndex)
  {
  }
  ArrayData(const ArrayData& aOther)
  {
    *this = aOther;
  }
  ArrayData& operator=(const ArrayData& aOther)
  {
    mIid = aOther.mIid;
    mMethodIndex = aOther.mMethodIndex;
    mArrayParamIndex = aOther.mArrayParamIndex;
    mArrayParamType = aOther.mArrayParamType;
    mArrayParamIid = aOther.mArrayParamIid;
    mLengthParamIndex = aOther.mLengthParamIndex;
    return *this;
  }
  IID     mIid;
  ULONG   mMethodIndex;
  ULONG   mArrayParamIndex;
  VARTYPE mArrayParamType;
  IID     mArrayParamIid;
  ULONG   mLengthParamIndex;
};

void RegisterArrayData(const ArrayData* aArrayData, size_t aLength);

template <size_t N>
inline void
RegisterArrayData(const ArrayData (&aData)[N])
{
  RegisterArrayData(aData, N);
}

const ArrayData*
FindArrayData(REFIID aIid, ULONG aMethodIndex);

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_Registration_h


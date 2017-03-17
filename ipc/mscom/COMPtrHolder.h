/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_COMPtrHolder_h
#define mozilla_mscom_COMPtrHolder_h

#include "mozilla/Attributes.h"
#include "mozilla/Move.h"
#include "mozilla/mscom/ProxyStream.h"
#include "mozilla/mscom/Ptr.h"

namespace mozilla {
namespace mscom {

template<typename Interface, const IID& _IID>
class COMPtrHolder
{
public:
  typedef ProxyUniquePtr<Interface> COMPtrType;
  typedef COMPtrHolder<Interface, _IID> ThisType;

  COMPtrHolder() {}

  MOZ_IMPLICIT COMPtrHolder(decltype(nullptr))
  {
  }

  explicit COMPtrHolder(COMPtrType&& aPtr)
    : mPtr(Forward<COMPtrType>(aPtr))
  {
  }

  Interface* Get() const
  {
    return mPtr.get();
  }

  MOZ_MUST_USE Interface* Release()
  {
    return mPtr.release();
  }

  void Set(COMPtrType&& aPtr)
  {
    mPtr = Forward<COMPtrType>(aPtr);
  }

  COMPtrHolder(const COMPtrHolder& aOther) = delete;

  COMPtrHolder(COMPtrHolder&& aOther)
    : mPtr(Move(aOther.mPtr))
  {
  }

  // COMPtrHolder is eventually added as a member of a struct that is declared
  // in IPDL. The generated C++ code for that IPDL struct includes copy
  // constructors and assignment operators that assume that all members are
  // copyable. I don't think that those copy constructors and operator= are
  // actually used by any generated code, but they are made available. Since no
  // move semantics are available, this terrible hack makes COMPtrHolder build
  // when used as a member of an IPDL struct.
  ThisType& operator=(const ThisType& aOther)
  {
    Set(Move(aOther.mPtr));
    return *this;
  }

  ThisType& operator=(ThisType&& aOther)
  {
    Set(Move(aOther.mPtr));
    return *this;
  }

  bool operator==(const ThisType& aOther) const
  {
    return mPtr == aOther.mPtr;
  }

  bool IsNull() const
  {
    return !mPtr;
  }

private:
  // This is mutable to facilitate the above operator= hack
  mutable COMPtrType mPtr;
};

} // namespace mscom
} // namespace mozilla

namespace IPC {

template<typename Interface, const IID& _IID>
struct ParamTraits<mozilla::mscom::COMPtrHolder<Interface, _IID>>
{
  typedef mozilla::mscom::COMPtrHolder<Interface, _IID> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    mozilla::mscom::ProxyStream proxyStream(_IID, aParam.Get());
    int bufLen;
    const BYTE* buf = proxyStream.GetBuffer(bufLen);
    MOZ_ASSERT(buf || !bufLen);
    aMsg->WriteInt(bufLen);
    if (bufLen) {
      aMsg->WriteBytes(reinterpret_cast<const char*>(buf), bufLen);
    }
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    int length;
    if (!aMsg->ReadLength(aIter, &length)) {
      return false;
    }

    mozilla::UniquePtr<BYTE[]> buf;
    if (length) {
      buf = mozilla::MakeUnique<BYTE[]>(length);
      if (!aMsg->ReadBytesInto(aIter, buf.get(), length)) {
        return false;
      }
    }

    mozilla::mscom::ProxyStream proxyStream(buf.get(), length);
    if (!proxyStream.IsValid()) {
      return false;
    }

    typename paramType::COMPtrType ptr;
    if (!proxyStream.GetInterface(_IID, mozilla::mscom::getter_AddRefs(ptr))) {
      return false;
    }

    aResult->Set(mozilla::Move(ptr));
    return true;
  }
};

} // namespace IPC

#endif // mozilla_mscom_COMPtrHolder_h


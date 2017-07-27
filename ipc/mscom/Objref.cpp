/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/Objref.h"

#include "mozilla/Assertions.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/UniquePtr.h"

#include <guiddef.h>
#include <objidl.h>

namespace {

#pragma pack(push, 1)

typedef uint64_t OID;
typedef uint64_t OXID;
typedef GUID IPID;

struct STDOBJREF
{
  uint32_t      mFlags;
  uint32_t      mPublicRefs;
  OXID          mOxid;
  OID           mOid;
  IPID          mIpid;
};

enum STDOBJREF_FLAGS
{
  SORF_PING = 0,
  SORF_NOPING = 0x1000
};

struct DUALSTRINGARRAY
{
  static size_t SizeFromNumEntries(const uint16_t aNumEntries)
  {
    return sizeof(mNumEntries) + sizeof(mSecurityOffset) +
           aNumEntries * sizeof(uint16_t);
  }

  size_t SizeOf() const
  {
    return SizeFromNumEntries(mNumEntries);
  }

  uint16_t  mNumEntries;
  uint16_t  mSecurityOffset;
  uint16_t  mStringArray[1]; // Length is mNumEntries
};

struct OBJREF_STANDARD
{
  size_t SizeOf() const
  {
    return sizeof(mStd) + mResAddr.SizeOf();
  }

  STDOBJREF       mStd;
  DUALSTRINGARRAY mResAddr;
};

struct OBJREF_HANDLER
{
  size_t SizeOf() const
  {
    return sizeof(mStd) + sizeof(mClsid) + mResAddr.SizeOf();
  }

  STDOBJREF       mStd;
  CLSID           mClsid;
  DUALSTRINGARRAY mResAddr;
};

enum OBJREF_FLAGS
{
  OBJREF_TYPE_STANDARD = 0x00000001UL,
  OBJREF_TYPE_HANDLER = 0x00000002UL,
  OBJREF_TYPE_CUSTOM = 0x00000004UL,
  OBJREF_TYPE_EXTENDED = 0x00000008UL,
};

struct OBJREF
{
  size_t SizeOf() const
  {
    size_t size = sizeof(mSignature) + sizeof(mFlags) + sizeof(mIid);

    switch (mFlags) {
      case OBJREF_TYPE_STANDARD:
        size += mObjRefStd.SizeOf();
        break;
      case OBJREF_TYPE_HANDLER:
        size += mObjRefHandler.SizeOf();
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported OBJREF type");
        return 0;
    }

    return size;
  }

  uint32_t  mSignature;
  uint32_t  mFlags;
  IID       mIid;
  union {
    OBJREF_STANDARD mObjRefStd;
    OBJREF_HANDLER  mObjRefHandler;
    // There are others but we're not supporting them here
  };
};

enum OBJREF_SIGNATURES
{
  OBJREF_SIGNATURE = 0x574F454DUL
};

#pragma pack(pop)

struct ByteArrayDeleter
{
  void operator()(void* aPtr)
  {
    delete[] reinterpret_cast<uint8_t*>(aPtr);
  }
};

template <typename T>
using VarStructUniquePtr = mozilla::UniquePtr<T, ByteArrayDeleter>;

} // anonymous namespace

namespace mozilla {
namespace mscom {

bool
StripHandlerFromOBJREF(NotNull<IStream*> aStream)
{
  // Get current stream position
  LARGE_INTEGER seekTo;
  seekTo.QuadPart = 0;

  ULARGE_INTEGER objrefPos;

  HRESULT hr = aStream->Seek(seekTo, STREAM_SEEK_CUR, &objrefPos);
  if (FAILED(hr)) {
    return false;
  }

  ULONG bytesRead;

  uint32_t signature;
  hr = aStream->Read(&signature, sizeof(signature), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(signature) ||
      signature != OBJREF_SIGNATURE) {
    return false;
  }

  uint32_t type;
  hr = aStream->Read(&type, sizeof(type), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(type) ||
      type != OBJREF_TYPE_HANDLER) {
    return false;
  }

  IID iid;
  hr = aStream->Read(&iid, sizeof(iid), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(iid) || !IsValidGUID(iid)) {
    return false;
  }

  // Seek past fixed-size STDOBJREF and CLSID
  seekTo.QuadPart = sizeof(STDOBJREF) + sizeof(CLSID);
  hr = aStream->Seek(seekTo, STREAM_SEEK_CUR, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  uint16_t numEntries;
  hr = aStream->Read(&numEntries, sizeof(numEntries), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(numEntries)) {
    return false;
  }

  // We'll try to use a stack buffer if resAddrSize <= kMinDualStringArraySize
  const uint32_t kMinDualStringArraySize = 12;
  uint16_t staticResAddrBuf[kMinDualStringArraySize / sizeof(uint16_t)];

  size_t resAddrSize = DUALSTRINGARRAY::SizeFromNumEntries(numEntries);

  DUALSTRINGARRAY* resAddr;
  VarStructUniquePtr<DUALSTRINGARRAY> dynamicResAddrBuf;

  if (resAddrSize <= kMinDualStringArraySize) {
    resAddr = reinterpret_cast<DUALSTRINGARRAY*>(staticResAddrBuf);
  } else {
    dynamicResAddrBuf.reset(
        reinterpret_cast<DUALSTRINGARRAY*>(new uint8_t[resAddrSize]));
    resAddr = dynamicResAddrBuf.get();
  }

  resAddr->mNumEntries = numEntries;

  // Because we've already read numEntries
  ULONG bytesToRead = resAddrSize - sizeof(numEntries);

  hr = aStream->Read(&resAddr->mSecurityOffset, bytesToRead, &bytesRead);
  if (FAILED(hr) || bytesRead != bytesToRead) {
    return false;
  }

  // Signature doesn't change so we'll seek past that
  seekTo.QuadPart = objrefPos.QuadPart + sizeof(signature);
  hr = aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return false;
  }

  ULONG bytesWritten;

  uint32_t newType = OBJREF_TYPE_STANDARD;
  hr = aStream->Write(&newType, sizeof(newType), &bytesWritten);
  if (FAILED(hr) || bytesWritten != sizeof(newType)) {
    return false;
  }

  // Skip past IID and STDOBJREF since those don't change
  seekTo.QuadPart = sizeof(IID) + sizeof(STDOBJREF);
  hr = aStream->Seek(seekTo, STREAM_SEEK_CUR, nullptr);
  if (FAILED(hr)) {
    return false;
  }

  hr = aStream->Write(resAddr, resAddrSize, &bytesWritten);
  if (FAILED(hr) || bytesWritten != resAddrSize) {
    return false;
  }

  // The difference between a OBJREF_STANDARD and an OBJREF_HANDLER is
  // sizeof(CLSID), so we'll zero out the remaining bytes.
  CLSID zeroClsid = {0};
  hr = aStream->Write(&zeroClsid, sizeof(CLSID), &bytesWritten);
  if (FAILED(hr) || bytesWritten != sizeof(CLSID)) {
    return false;
  }

  return true;
}

} // namespace mscom
} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/Objref.h"

#include "mozilla/Assertions.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtr.h"

#include <guiddef.h>
#include <objidl.h>
#include <winnt.h>

// {00000027-0000-0008-C000-000000000046}
static const GUID CLSID_AggStdMarshal = {
    0x27, 0x0, 0x8, {0xC0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x46}};

namespace {

#pragma pack(push, 1)

typedef uint64_t OID;
typedef uint64_t OXID;
typedef GUID IPID;

struct STDOBJREF {
  uint32_t mFlags;
  uint32_t mPublicRefs;
  OXID mOxid;
  OID mOid;
  IPID mIpid;
};

enum STDOBJREF_FLAGS { SORF_PING = 0, SORF_NOPING = 0x1000 };

struct DUALSTRINGARRAY {
  static size_t SizeFromNumEntries(const uint16_t aNumEntries) {
    return sizeof(mNumEntries) + sizeof(mSecurityOffset) +
           aNumEntries * sizeof(uint16_t);
  }

  size_t SizeOf() const { return SizeFromNumEntries(mNumEntries); }

  uint16_t mNumEntries;
  uint16_t mSecurityOffset;
  uint16_t mStringArray[1];  // Length is mNumEntries
};

struct OBJREF_STANDARD {
  static size_t SizeOfFixedLenHeader() { return sizeof(mStd); }

  size_t SizeOf() const { return SizeOfFixedLenHeader() + mResAddr.SizeOf(); }

  STDOBJREF mStd;
  DUALSTRINGARRAY mResAddr;
};

struct OBJREF_HANDLER {
  static size_t SizeOfFixedLenHeader() { return sizeof(mStd) + sizeof(mClsid); }

  size_t SizeOf() const { return SizeOfFixedLenHeader() + mResAddr.SizeOf(); }

  STDOBJREF mStd;
  CLSID mClsid;
  DUALSTRINGARRAY mResAddr;
};

struct OBJREF_CUSTOM {
  static size_t SizeOfFixedLenHeader() {
    return sizeof(mClsid) + sizeof(mCbExtension) + sizeof(mReserved);
  }

  CLSID mClsid;
  uint32_t mCbExtension;
  uint32_t mReserved;
  uint8_t mPayload[1];
};

enum OBJREF_FLAGS {
  OBJREF_TYPE_STANDARD = 0x00000001UL,
  OBJREF_TYPE_HANDLER = 0x00000002UL,
  OBJREF_TYPE_CUSTOM = 0x00000004UL,
  OBJREF_TYPE_EXTENDED = 0x00000008UL,
};

struct OBJREF {
  static size_t SizeOfFixedLenHeader(OBJREF_FLAGS aFlags) {
    size_t size = sizeof(mSignature) + sizeof(mFlags) + sizeof(mIid);

    switch (aFlags) {
      case OBJREF_TYPE_STANDARD:
        size += OBJREF_STANDARD::SizeOfFixedLenHeader();
        break;
      case OBJREF_TYPE_HANDLER:
        size += OBJREF_HANDLER::SizeOfFixedLenHeader();
        break;
      case OBJREF_TYPE_CUSTOM:
        size += OBJREF_CUSTOM::SizeOfFixedLenHeader();
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unsupported OBJREF type");
        return 0;
    }

    return size;
  }

  size_t SizeOf() const {
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

  uint32_t mSignature;
  uint32_t mFlags;
  IID mIid;
  union {
    OBJREF_STANDARD mObjRefStd;
    OBJREF_HANDLER mObjRefHandler;
    OBJREF_CUSTOM mObjRefCustom;
    // There are others but we're not supporting them here
  };
};

enum OBJREF_SIGNATURES { OBJREF_SIGNATURE = 0x574F454DUL };

#pragma pack(pop)

struct ByteArrayDeleter {
  void operator()(void* aPtr) { delete[] reinterpret_cast<uint8_t*>(aPtr); }
};

template <typename T>
using VarStructUniquePtr = mozilla::UniquePtr<T, ByteArrayDeleter>;

}  // anonymous namespace

namespace mozilla {
namespace mscom {

bool StripHandlerFromOBJREF(NotNull<IStream*> aStream, const uint64_t aStartPos,
                            const uint64_t aEndPos) {
  // Ensure that the current stream position is set to the beginning
  LARGE_INTEGER seekTo;
  seekTo.QuadPart = aStartPos;

  HRESULT hr = aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr);
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
  if (FAILED(hr) || bytesRead != sizeof(type)) {
    return false;
  }
  if (type != OBJREF_TYPE_HANDLER) {
    // If we're not a handler then just seek to the end of the OBJREF and return
    // success; there is nothing left to do.
    seekTo.QuadPart = aEndPos;
    return SUCCEEDED(aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr));
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
    return false;
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
  seekTo.QuadPart = aStartPos + sizeof(signature);
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
  hr = aStream->Write(&CLSID_NULL, sizeof(CLSID), &bytesWritten);
  if (FAILED(hr) || bytesWritten != sizeof(CLSID)) {
    return false;
  }

  // Back up to the end of the tweaked OBJREF.
  // There are now sizeof(CLSID) less bytes.
  // Bug 1403180: Using -sizeof(CLSID) with a relative seek sometimes
  // doesn't work on Windows 7.
  // It succeeds, but doesn't seek the stream for some unknown reason.
  // Use an absolute seek instead.
  seekTo.QuadPart = aEndPos - sizeof(CLSID);
  return SUCCEEDED(aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr));
}

uint32_t GetOBJREFSize(NotNull<IStream*> aStream) {
  // Make a clone so that we don't manipulate aStream's seek pointer
  RefPtr<IStream> cloned;
  HRESULT hr = aStream->Clone(getter_AddRefs(cloned));
  if (FAILED(hr)) {
    return 0;
  }

  uint32_t accumulatedSize = 0;

  ULONG bytesRead;

  uint32_t signature;
  hr = cloned->Read(&signature, sizeof(signature), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(signature) ||
      signature != OBJREF_SIGNATURE) {
    return 0;
  }

  accumulatedSize += bytesRead;

  uint32_t type;
  hr = cloned->Read(&type, sizeof(type), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(type)) {
    return 0;
  }

  accumulatedSize += bytesRead;

  IID iid;
  hr = cloned->Read(&iid, sizeof(iid), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(iid) || !IsValidGUID(iid)) {
    return 0;
  }

  accumulatedSize += bytesRead;

  LARGE_INTEGER seekTo;

  if (type == OBJREF_TYPE_CUSTOM) {
    CLSID clsid;
    hr = cloned->Read(&clsid, sizeof(CLSID), &bytesRead);
    if (FAILED(hr) || bytesRead != sizeof(CLSID)) {
      return 0;
    }

    if (clsid != CLSID_StdMarshal && clsid != CLSID_AggStdMarshal) {
      // We can only calulate the size if the payload is a standard OBJREF as
      // identified by clsid being CLSID_StdMarshal or CLSID_AggStdMarshal.
      // (CLSID_AggStdMarshal, the aggregated standard marshaler, is used when
      // the handler marshals an interface.)
      return 0;
    }

    accumulatedSize += bytesRead;

    seekTo.QuadPart =
        sizeof(OBJREF_CUSTOM::mCbExtension) + sizeof(OBJREF_CUSTOM::mReserved);
    hr = cloned->Seek(seekTo, STREAM_SEEK_CUR, nullptr);
    if (FAILED(hr)) {
      return 0;
    }

    accumulatedSize += seekTo.LowPart;

    uint32_t payloadLen = GetOBJREFSize(WrapNotNull(cloned.get()));
    if (!payloadLen) {
      return 0;
    }

    accumulatedSize += payloadLen;
    return accumulatedSize;
  }

  switch (type) {
    case OBJREF_TYPE_STANDARD:
      seekTo.QuadPart = OBJREF_STANDARD::SizeOfFixedLenHeader();
      break;
    case OBJREF_TYPE_HANDLER:
      seekTo.QuadPart = OBJREF_HANDLER::SizeOfFixedLenHeader();
      break;
    default:
      return 0;
  }

  hr = cloned->Seek(seekTo, STREAM_SEEK_CUR, nullptr);
  if (FAILED(hr)) {
    return 0;
  }

  accumulatedSize += seekTo.LowPart;

  uint16_t numEntries;
  hr = cloned->Read(&numEntries, sizeof(numEntries), &bytesRead);
  if (FAILED(hr) || bytesRead != sizeof(numEntries)) {
    return 0;
  }

  accumulatedSize += DUALSTRINGARRAY::SizeFromNumEntries(numEntries);
  return accumulatedSize;
}

bool SetIID(NotNull<IStream*> aStream, const uint64_t aStart, REFIID aNewIid) {
  ULARGE_INTEGER initialStreamPos;

  LARGE_INTEGER seekTo;
  seekTo.QuadPart = 0LL;
  HRESULT hr = aStream->Seek(seekTo, STREAM_SEEK_CUR, &initialStreamPos);
  if (FAILED(hr)) {
    return false;
  }

  auto resetStreamPos = MakeScopeExit([&]() {
    seekTo.QuadPart = initialStreamPos.QuadPart;
    hr = aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr);
    MOZ_DIAGNOSTIC_ASSERT(SUCCEEDED(hr));
  });

  seekTo.QuadPart =
      aStart + sizeof(OBJREF::mSignature) + sizeof(OBJREF::mFlags);
  hr = aStream->Seek(seekTo, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return false;
  }

  ULONG bytesWritten;
  hr = aStream->Write(&aNewIid, sizeof(IID), &bytesWritten);
  return SUCCEEDED(hr) && bytesWritten == sizeof(IID);
}

}  // namespace mscom
}  // namespace mozilla

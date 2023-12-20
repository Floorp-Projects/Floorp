/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  include "MainThreadUtils.h"
#  include "mozilla/dom/ContentChild.h"
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"

#include <objidl.h>
#include <winnt.h>

#include <utility>

#if defined(_MSC_VER)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace mozilla {
namespace mscom {

bool IsCOMInitializedOnCurrentThread() {
  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = wrapped::CoGetApartmentType(&aptType, &aptTypeQualifier);
  return hr != CO_E_NOTINITIALIZED;
}

bool IsCurrentThreadMTA() {
  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = wrapped::CoGetApartmentType(&aptType, &aptTypeQualifier);
  if (FAILED(hr)) {
    return false;
  }

  return aptType == APTTYPE_MTA;
}

bool IsCurrentThreadExplicitMTA() {
  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = wrapped::CoGetApartmentType(&aptType, &aptTypeQualifier);
  if (FAILED(hr)) {
    return false;
  }

  return aptType == APTTYPE_MTA &&
         aptTypeQualifier != APTTYPEQUALIFIER_IMPLICIT_MTA;
}

bool IsCurrentThreadImplicitMTA() {
  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = wrapped::CoGetApartmentType(&aptType, &aptTypeQualifier);
  if (FAILED(hr)) {
    return false;
  }

  return aptType == APTTYPE_MTA &&
         aptTypeQualifier == APTTYPEQUALIFIER_IMPLICIT_MTA;
}

#if defined(MOZILLA_INTERNAL_API)
bool IsCurrentThreadNonMainMTA() {
  if (NS_IsMainThread()) {
    return false;
  }

  return IsCurrentThreadMTA();
}
#endif  // defined(MOZILLA_INTERNAL_API)

bool IsProxy(IUnknown* aUnknown) {
  if (!aUnknown) {
    return false;
  }

  // Only proxies implement this interface, so if it is present then we must
  // be dealing with a proxied object.
  RefPtr<IClientSecurity> clientSecurity;
  HRESULT hr = aUnknown->QueryInterface(IID_IClientSecurity,
                                        (void**)getter_AddRefs(clientSecurity));
  if (SUCCEEDED(hr) || hr == RPC_E_WRONG_THREAD) {
    return true;
  }
  return false;
}

bool IsValidGUID(REFGUID aCheckGuid) {
  // This function determines whether or not aCheckGuid conforms to RFC4122
  // as it applies to Microsoft COM.

  BYTE variant = aCheckGuid.Data4[0];
  if (!(variant & 0x80)) {
    // NCS Reserved
    return false;
  }
  if ((variant & 0xE0) == 0xE0) {
    // Reserved for future use
    return false;
  }
  if ((variant & 0xC0) == 0xC0) {
    // Microsoft Reserved.
    return true;
  }

  BYTE version = HIBYTE(aCheckGuid.Data3) >> 4;
  // Other versions are specified in RFC4122 but these are the two used by COM.
  return version == 1 || version == 4;
}

uintptr_t GetContainingModuleHandle() {
  HMODULE thisModule = nullptr;
#if defined(_MSC_VER)
  thisModule = reinterpret_cast<HMODULE>(&__ImageBase);
#else
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCTSTR>(&GetContainingModuleHandle),
                         &thisModule)) {
    return 0;
  }
#endif
  return reinterpret_cast<uintptr_t>(thisModule);
}

namespace detail {

long BuildRegGuidPath(REFGUID aGuid, const GuidType aGuidType, wchar_t* aBuf,
                      const size_t aBufLen) {
  constexpr wchar_t kClsid[] = L"CLSID\\";
  constexpr wchar_t kAppid[] = L"AppID\\";
  constexpr wchar_t kSubkeyBase[] = L"SOFTWARE\\Classes\\";

  // We exclude null terminators in these length calculations because we include
  // the stringified GUID's null terminator at the end. Since kClsid and kAppid
  // have identical lengths, we just choose one to compute this length.
  constexpr size_t kSubkeyBaseLen = mozilla::ArrayLength(kSubkeyBase) - 1;
  constexpr size_t kSubkeyLen =
      kSubkeyBaseLen + mozilla::ArrayLength(kClsid) - 1;
  // Guid length as formatted for the registry (including curlies and dashes),
  // but excluding null terminator.
  constexpr size_t kGuidLen = kGuidRegFormatCharLenInclNul - 1;
  constexpr size_t kExpectedPathLenInclNul = kSubkeyLen + kGuidLen + 1;

  if (aBufLen < kExpectedPathLenInclNul) {
    // Buffer is too short
    return E_INVALIDARG;
  }

  if (wcscpy_s(aBuf, aBufLen, kSubkeyBase)) {
    return E_INVALIDARG;
  }

  const wchar_t* strGuidType = aGuidType == GuidType::CLSID ? kClsid : kAppid;
  if (wcscat_s(aBuf, aBufLen, strGuidType)) {
    return E_INVALIDARG;
  }

  int guidConversionResult =
      ::StringFromGUID2(aGuid, &aBuf[kSubkeyLen], aBufLen - kSubkeyLen);
  if (!guidConversionResult) {
    return E_INVALIDARG;
  }

  return S_OK;
}

}  // namespace detail

long CreateStream(const uint8_t* aInitBuf, const uint32_t aInitBufSize,
                  IStream** aOutStream) {
  if (!aInitBufSize || !aOutStream) {
    return E_INVALIDARG;
  }

  *aOutStream = nullptr;

  HRESULT hr;
  RefPtr<IStream> stream;

  // If aInitBuf is null then initSize must be 0.
  UINT initSize = aInitBuf ? aInitBufSize : 0;
  stream = already_AddRefed<IStream>(::SHCreateMemStream(aInitBuf, initSize));
  if (!stream) {
    return E_OUTOFMEMORY;
  }

  if (!aInitBuf) {
    // Now we'll set the required size
    ULARGE_INTEGER newSize;
    newSize.QuadPart = aInitBufSize;
    hr = stream->SetSize(newSize);
    if (FAILED(hr)) {
      return hr;
    }
  }

  // Ensure that the stream is rewound
  LARGE_INTEGER streamOffset;
  streamOffset.QuadPart = 0LL;
  hr = stream->Seek(streamOffset, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  stream.forget(aOutStream);
  return S_OK;
}

#if defined(MOZILLA_INTERNAL_API)

void GUIDToString(REFGUID aGuid, nsAString& aOutString) {
  // This buffer length is long enough to hold a GUID string that is formatted
  // to include curly braces and dashes.
  const int kBufLenWithNul = 39;
  aOutString.SetLength(kBufLenWithNul);
  int result = StringFromGUID2(aGuid, char16ptr_t(aOutString.BeginWriting()),
                               kBufLenWithNul);
  MOZ_ASSERT(result);
  if (result) {
    // Truncate the terminator
    aOutString.SetLength(result - 1);
  }
}

// Undocumented IIDs that are relevant for diagnostic purposes
static const IID IID_ISCMLocalActivator = {
    0x00000136,
    0x0000,
    0x0000,
    {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_IRundown = {
    0x00000134,
    0x0000,
    0x0000,
    {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_IRemUnknown = {
    0x00000131,
    0x0000,
    0x0000,
    {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const IID IID_IRemUnknown2 = {
    0x00000143,
    0x0000,
    0x0000,
    {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct IIDToLiteralMapEntry {
  constexpr IIDToLiteralMapEntry(REFIID aIid, nsLiteralCString&& aStr)
      : mIid(aIid), mStr(std::forward<nsLiteralCString>(aStr)) {}

  REFIID mIid;
  const nsLiteralCString mStr;
};

/**
 * Given the name of an interface, the IID_ENTRY macro generates a pair
 * containing a reference to the interface ID and a stringified version of
 * the interface name.
 *
 * For example:
 *
 *   {IID_ENTRY(IUnknown)}
 * is expanded to:
 *   {IID_IUnknown, "IUnknown"_ns}
 *
 */
// clang-format off
#  define IID_ENTRY_STRINGIFY(iface) #iface##_ns
#  define IID_ENTRY(iface) IID_##iface, IID_ENTRY_STRINGIFY(iface)
// clang-format on

// Mapping of selected IIDs to friendly, human readable descriptions for each
// interface.
static constexpr IIDToLiteralMapEntry sIidDiagStrs[] = {
    {IID_ENTRY(IUnknown)},
    {IID_IRemUnknown, "cross-apartment IUnknown"_ns},
    {IID_IRundown, "cross-apartment object management"_ns},
    {IID_ISCMLocalActivator, "out-of-process object instantiation"_ns},
    {IID_IRemUnknown2, "cross-apartment IUnknown"_ns}};

#  undef IID_ENTRY
#  undef IID_ENTRY_STRINGIFY

void DiagnosticNameForIID(REFIID aIid, nsACString& aOutString) {
  // If the IID matches something in sIidDiagStrs, output its string.
  for (const auto& curEntry : sIidDiagStrs) {
    if (curEntry.mIid == aIid) {
      aOutString.Assign(curEntry.mStr);
      return;
    }
  }

  // Otherwise just convert the IID to string form and output that.
  nsAutoString strIid;
  GUIDToString(aIid, strIid);

  aOutString.AssignLiteral("IID ");
  AppendUTF16toUTF8(strIid, aOutString);
}

#else

void GUIDToString(REFGUID aGuid,
                  wchar_t (&aOutBuf)[kGuidRegFormatCharLenInclNul]) {
  DebugOnly<int> result =
      ::StringFromGUID2(aGuid, aOutBuf, ArrayLength(aOutBuf));
  MOZ_ASSERT(result);
}

#endif  // defined(MOZILLA_INTERNAL_API)

}  // namespace mscom
}  // namespace mozilla

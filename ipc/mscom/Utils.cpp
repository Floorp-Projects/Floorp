/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  include "MainThreadUtils.h"
#  include "mozilla/dom/ContentChild.h"
#endif

#if defined(ACCESSIBILITY)
#  include "mozilla/mscom/Registration.h"
#  if defined(MOZILLA_INTERNAL_API)
#    include "nsTArray.h"
#  endif
#endif

#include "mozilla/ArrayUtils.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/Objref.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WindowsVersion.h"

#include <objidl.h>
#include <shlwapi.h>
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

  if (IsWin8OrLater()) {
    // SHCreateMemStream is not safe for us to use until Windows 8. On older
    // versions of Windows it is not thread-safe and it creates IStreams that do
    // not support the full IStream API.

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
  } else {
    HGLOBAL hglobal = ::GlobalAlloc(GMEM_MOVEABLE, aInitBufSize);
    if (!hglobal) {
      return HRESULT_FROM_WIN32(::GetLastError());
    }

    // stream takes ownership of hglobal if this call is successful
    hr = ::CreateStreamOnHGlobal(hglobal, TRUE, getter_AddRefs(stream));
    if (FAILED(hr)) {
      ::GlobalFree(hglobal);
      return hr;
    }

    // The default stream size is derived from ::GlobalSize(hglobal), which due
    // to rounding may be larger than aInitBufSize. We forcibly set the correct
    // stream size here.
    ULARGE_INTEGER streamSize;
    streamSize.QuadPart = aInitBufSize;
    hr = stream->SetSize(streamSize);
    if (FAILED(hr)) {
      return hr;
    }

    if (aInitBuf) {
      ULONG bytesWritten;
      hr = stream->Write(aInitBuf, aInitBufSize, &bytesWritten);
      if (FAILED(hr)) {
        return hr;
      }

      if (bytesWritten != aInitBufSize) {
        return E_UNEXPECTED;
      }
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

long CopySerializedProxy(IStream* aInStream, IStream** aOutStream) {
  if (!aInStream || !aOutStream) {
    return E_INVALIDARG;
  }

  *aOutStream = nullptr;

  uint32_t desiredStreamSize = GetOBJREFSize(WrapNotNull(aInStream));
  if (!desiredStreamSize) {
    return E_INVALIDARG;
  }

  RefPtr<IStream> stream;
  HRESULT hr = CreateStream(nullptr, desiredStreamSize, getter_AddRefs(stream));
  if (FAILED(hr)) {
    return hr;
  }

  ULARGE_INTEGER numBytesToCopy;
  numBytesToCopy.QuadPart = desiredStreamSize;
  hr = aInStream->CopyTo(stream, numBytesToCopy, nullptr, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  LARGE_INTEGER seekTo;
  seekTo.QuadPart = 0LL;
  hr = stream->Seek(seekTo, STREAM_SEEK_SET, nullptr);
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

#if defined(ACCESSIBILITY)

static bool IsVtableIndexFromParentInterface(TYPEATTR* aTypeAttr,
                                             unsigned long aVtableIndex) {
  MOZ_ASSERT(aTypeAttr);

  // This is the number of functions declared in this interface (excluding
  // parent interfaces).
  unsigned int numExclusiveFuncs = aTypeAttr->cFuncs;

  // This is the number of vtable entries (which includes parent interfaces).
  // TYPEATTR::cbSizeVft is the entire vtable size in bytes, so we need to
  // divide in order to compute the number of entries.
  unsigned int numVtblEntries = aTypeAttr->cbSizeVft / sizeof(void*);

  // This is the index of the first entry in the vtable that belongs to this
  // interface and not a parent.
  unsigned int firstVtblIndex = numVtblEntries - numExclusiveFuncs;

  // If aVtableIndex is less than firstVtblIndex, then we're asking for an
  // index that may belong to a parent interface.
  return aVtableIndex < firstVtblIndex;
}

bool IsVtableIndexFromParentInterface(REFIID aInterface,
                                      unsigned long aVtableIndex) {
  RefPtr<ITypeInfo> typeInfo;
  if (!RegisteredProxy::Find(aInterface, getter_AddRefs(typeInfo))) {
    return false;
  }

  TYPEATTR* typeAttr = nullptr;
  HRESULT hr = typeInfo->GetTypeAttr(&typeAttr);
  if (FAILED(hr)) {
    return false;
  }

  bool result = IsVtableIndexFromParentInterface(typeAttr, aVtableIndex);

  typeInfo->ReleaseTypeAttr(typeAttr);
  return result;
}

#  if defined(MOZILLA_INTERNAL_API)

bool IsCallerExternalProcess() {
  MOZ_ASSERT(XRE_IsContentProcess());

  /**
   * CoGetCallerTID() gives us the caller's thread ID when that thread resides
   * in a single-threaded apartment. Since our chrome main thread does live
   * inside an STA, we will therefore be able to check whether the caller TID
   * equals our chrome main thread TID. This enables us to distinguish
   * between our chrome thread vs other out-of-process callers. We check for
   * S_FALSE to ensure that the caller is a different process from ours, which
   * is the only scenario that we care about.
   */
  DWORD callerTid;
  if (::CoGetCallerTID(&callerTid) != S_FALSE) {
    return false;
  }

  // Now check whether the caller is our parent process main thread.
  const DWORD parentMainTid =
      dom::ContentChild::GetSingleton()->GetChromeMainThreadId();
  return callerTid != parentMainTid;
}

bool IsInterfaceEqualToOrInheritedFrom(REFIID aInterface, REFIID aFrom,
                                       unsigned long aVtableIndexHint) {
  if (aInterface == aFrom) {
    return true;
  }

  // We expect this array to be length 1 but that is not guaranteed by the API.
  AutoTArray<RefPtr<ITypeInfo>, 1> typeInfos;

  // Grab aInterface's ITypeInfo so that we may obtain information about its
  // inheritance hierarchy.
  RefPtr<ITypeInfo> typeInfo;
  if (RegisteredProxy::Find(aInterface, getter_AddRefs(typeInfo))) {
    typeInfos.AppendElement(std::move(typeInfo));
  }

  /**
   * The main loop of this function searches the hierarchy of aInterface's
   * parent interfaces, searching for aFrom.
   */
  while (!typeInfos.IsEmpty()) {
    RefPtr<ITypeInfo> curTypeInfo(typeInfos.PopLastElement());

    TYPEATTR* typeAttr = nullptr;
    HRESULT hr = curTypeInfo->GetTypeAttr(&typeAttr);
    if (FAILED(hr)) {
      break;
    }

    bool isFromParentVtable =
        IsVtableIndexFromParentInterface(typeAttr, aVtableIndexHint);
    WORD numParentInterfaces = typeAttr->cImplTypes;

    curTypeInfo->ReleaseTypeAttr(typeAttr);
    typeAttr = nullptr;

    if (!isFromParentVtable) {
      // The vtable index cannot belong to this interface (otherwise the IIDs
      // would already have matched and we would have returned true). Since we
      // now also know that the vtable index cannot possibly be contained inside
      // curTypeInfo's parent interface, there is no point searching any further
      // up the hierarchy from here. OTOH we still should check any remaining
      // entries that are still in the typeInfos array, so we continue.
      continue;
    }

    for (WORD i = 0; i < numParentInterfaces; ++i) {
      HREFTYPE refCookie;
      hr = curTypeInfo->GetRefTypeOfImplType(i, &refCookie);
      if (FAILED(hr)) {
        continue;
      }

      RefPtr<ITypeInfo> nextTypeInfo;
      hr = curTypeInfo->GetRefTypeInfo(refCookie, getter_AddRefs(nextTypeInfo));
      if (FAILED(hr)) {
        continue;
      }

      hr = nextTypeInfo->GetTypeAttr(&typeAttr);
      if (FAILED(hr)) {
        continue;
      }

      IID nextIid = typeAttr->guid;

      nextTypeInfo->ReleaseTypeAttr(typeAttr);
      typeAttr = nullptr;

      if (nextIid == aFrom) {
        return true;
      }

      typeInfos.AppendElement(std::move(nextTypeInfo));
    }
  }

  return false;
}

#  endif  // defined(MOZILLA_INTERNAL_API)

#endif  // defined(ACCESSIBILITY)

#if defined(MOZILLA_INTERNAL_API)
bool IsClassThreadAwareInprocServer(REFCLSID aClsid) {
  nsAutoString strClsid;
  GUIDToString(aClsid, strClsid);

  nsAutoString inprocServerSubkey(u"CLSID\\"_ns);
  inprocServerSubkey.Append(strClsid);
  inprocServerSubkey.Append(u"\\InprocServer32"_ns);

  // Of the possible values, "Apartment" is the longest, so we'll make this
  // buffer large enough to hold that one.
  wchar_t threadingModelBuf[ArrayLength(L"Apartment")] = {};

  DWORD numBytes = sizeof(threadingModelBuf);
  LONG result = ::RegGetValueW(HKEY_CLASSES_ROOT, inprocServerSubkey.get(),
                               L"ThreadingModel", RRF_RT_REG_SZ, nullptr,
                               threadingModelBuf, &numBytes);
  if (result != ERROR_SUCCESS) {
    // This will also handle the case where the CLSID is not an inproc server.
    return false;
  }

  DWORD numChars = numBytes / sizeof(wchar_t);
  // numChars includes the null terminator
  if (numChars <= 1) {
    return false;
  }

  nsDependentString threadingModel(threadingModelBuf, numChars - 1);

  // Ensure that the threading model is one of the known values that indicates
  // that the class can operate natively (ie, no proxying) inside a MTA.
  return threadingModel.LowerCaseEqualsLiteral("both") ||
         threadingModel.LowerCaseEqualsLiteral("free") ||
         threadingModel.LowerCaseEqualsLiteral("neutral");
}
#endif  // defined(MOZILLA_INTERNAL_API)

}  // namespace mscom
}  // namespace mozilla

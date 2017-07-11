/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(ACCESSIBILITY)
#include "mozilla/mscom/Registration.h"
#if defined(MOZILLA_INTERNAL_API)
#include "nsTArray.h"
#endif
#endif

#include "mozilla/mscom/Utils.h"
#include "mozilla/RefPtr.h"

#include <objbase.h>
#include <objidl.h>
#include <winnt.h>

#if defined(_MSC_VER)
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif

namespace mozilla {
namespace mscom {

bool
IsCurrentThreadMTA()
{
  APTTYPE aptType;
  APTTYPEQUALIFIER aptTypeQualifier;
  HRESULT hr = CoGetApartmentType(&aptType, &aptTypeQualifier);
  if (FAILED(hr)) {
    return false;
  }

  return aptType == APTTYPE_MTA;
}

bool
IsProxy(IUnknown* aUnknown)
{
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

bool
IsValidGUID(REFGUID aCheckGuid)
{
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

uintptr_t
GetContainingModuleHandle()
{
  HMODULE thisModule = nullptr;
#if defined(_MSC_VER)
  thisModule = reinterpret_cast<HMODULE>(&__ImageBase);
#else
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCTSTR>(&GetContainingModuleHandle),
                         &thisModule)) {
    return 0;
  }
#endif
  return reinterpret_cast<uintptr_t>(thisModule);
}

#if defined(MOZILLA_INTERNAL_API)

void
GUIDToString(REFGUID aGuid, nsAString& aOutString)
{
  // This buffer length is long enough to hold a GUID string that is formatted
  // to include curly braces and dashes.
  const int kBufLenWithNul = 39;
  aOutString.SetLength(kBufLenWithNul);
  int result = StringFromGUID2(aGuid, char16ptr_t(aOutString.BeginWriting()), kBufLenWithNul);
  MOZ_ASSERT(result);
  if (result) {
    // Truncate the terminator
    aOutString.SetLength(result - 1);
  }
}

#endif // defined(MOZILLA_INTERNAL_API)

#if defined(ACCESSIBILITY)

static bool
IsVtableIndexFromParentInterface(TYPEATTR* aTypeAttr,
                                 unsigned long aVtableIndex)
{
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

bool
IsVtableIndexFromParentInterface(REFIID aInterface, unsigned long aVtableIndex)
{
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

#if defined(MOZILLA_INTERNAL_API)

bool
IsInterfaceEqualToOrInheritedFrom(REFIID aInterface, REFIID aFrom,
                                  unsigned long aVtableIndexHint)
{
  if (aInterface == aFrom) {
    return true;
  }

  // We expect this array to be length 1 but that is not guaranteed by the API.
  AutoTArray<RefPtr<ITypeInfo>, 1> typeInfos;

  // Grab aInterface's ITypeInfo so that we may obtain information about its
  // inheritance hierarchy.
  RefPtr<ITypeInfo> typeInfo;
  if (RegisteredProxy::Find(aInterface, getter_AddRefs(typeInfo))) {
    typeInfos.AppendElement(Move(typeInfo));
  }

  /**
   * The main loop of this function searches the hierarchy of aInterface's
   * parent interfaces, searching for aFrom.
   */
  while (!typeInfos.IsEmpty()) {
    RefPtr<ITypeInfo> curTypeInfo(Move(typeInfos.LastElement()));
    typeInfos.RemoveElementAt(typeInfos.Length() - 1);

    TYPEATTR* typeAttr = nullptr;
    HRESULT hr = curTypeInfo->GetTypeAttr(&typeAttr);
    if (FAILED(hr)) {
      break;
    }

    bool isFromParentVtable = IsVtableIndexFromParentInterface(typeAttr,
                                                               aVtableIndexHint);
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
      hr = curTypeInfo->GetRefTypeInfo(refCookie,
                                       getter_AddRefs(nextTypeInfo));
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

      typeInfos.AppendElement(Move(nextTypeInfo));
    }
  }

  return false;
}

#endif // defined(MOZILLA_INTERNAL_API)

#endif // defined(ACCESSIBILITY)

} // namespace mscom
} // namespace mozilla

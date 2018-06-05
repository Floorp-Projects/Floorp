/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_NativeNt_h
#define mozilla_NativeNt_h

#if defined(MOZILLA_INTERNAL_API)
#error "This header is for initial process initialization. You don't want to be including this here."
#endif // defined(MOZILLA_INTERNAL_API)

#include <stdint.h>
#include <windows.h>
#include <winnt.h>
#include <winternl.h>

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"

extern "C" {

#if !defined(STATUS_ACCESS_DENIED)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)
#endif // !defined(STATUS_ACCESS_DENIED)

#if !defined(STATUS_DLL_NOT_FOUND)
#define STATUS_DLL_NOT_FOUND ((NTSTATUS)0xC0000135L)
#endif // !defined(STATUS_DLL_NOT_FOUND)

enum SECTION_INHERIT
{
  ViewShare = 1,
  ViewUnmap = 2
};

NTSTATUS NTAPI
NtMapViewOfSection(HANDLE aSection, HANDLE aProcess, PVOID* aBaseAddress,
                   ULONG_PTR aZeroBits, SIZE_T aCommitSize,
                   PLARGE_INTEGER aSectionOffset, PSIZE_T aViewSize,
                   SECTION_INHERIT aInheritDisposition, ULONG aAllocationType,
                   ULONG aProtectionFlags);

NTSTATUS NTAPI
NtUnmapViewOfSection(HANDLE aProcess, PVOID aBaseAddress);

enum MEMORY_INFORMATION_CLASS
{
  MemoryBasicInformation = 0,
  MemorySectionName = 2
};

// NB: When allocating, space for the buffer must also be included
typedef struct _MEMORY_SECTION_NAME
{
  UNICODE_STRING mSectionFileName;
} MEMORY_SECTION_NAME, *PMEMORY_SECTION_NAME;

NTSTATUS NTAPI
NtQueryVirtualMemory(HANDLE aProcess, PVOID aBaseAddress,
                     MEMORY_INFORMATION_CLASS aMemInfoClass, PVOID aMemInfo,
                     SIZE_T aMemInfoLen, PSIZE_T aReturnLen);

LONG NTAPI
RtlCompareUnicodeString(PCUNICODE_STRING aStr1, PCUNICODE_STRING aStr2,
                        BOOLEAN aCaseInsensitive);

BOOLEAN NTAPI
RtlEqualUnicodeString(PCUNICODE_STRING aStr1, PCUNICODE_STRING aStr2,
                      BOOLEAN aCaseInsensitive);

NTSTATUS NTAPI
RtlGetVersion(PRTL_OSVERSIONINFOW aOutVersionInformation);

PVOID NTAPI
RtlAllocateHeap(PVOID aHeapHandle, ULONG aFlags, SIZE_T aSize);

PVOID NTAPI
RtlReAllocateHeap(PVOID aHeapHandle, ULONG aFlags, LPVOID aMem, SIZE_T aNewSize);

BOOLEAN NTAPI
RtlFreeHeap(PVOID aHeapHandle, ULONG aFlags, PVOID aHeapBase);

VOID NTAPI
RtlAcquireSRWLockExclusive(PSRWLOCK aLock);

VOID NTAPI
RtlReleaseSRWLockExclusive(PSRWLOCK aLock);

} // extern "C"

namespace mozilla {
namespace nt {

struct MemorySectionNameBuf : public _MEMORY_SECTION_NAME
{
  MemorySectionNameBuf()
  {
    mSectionFileName.Length = 0;
    mSectionFileName.MaximumLength = sizeof(mBuf);
    mSectionFileName.Buffer = mBuf;
  }

  WCHAR mBuf[MAX_PATH];
};

inline bool
FindCharInUnicodeString(const UNICODE_STRING& aStr, WCHAR aChar, uint16_t& aPos,
                        uint16_t aStartIndex = 0)
{
  const uint16_t aMaxIndex = aStr.Length / sizeof(WCHAR);

  for (uint16_t curIndex = aStartIndex; curIndex < aMaxIndex; ++curIndex) {
    if (aStr.Buffer[curIndex] == aChar) {
      aPos = curIndex;
      return true;
    }
  }

  return false;
}

inline bool
IsHexDigit(WCHAR aChar)
{
  return aChar >= L'0' && aChar <= L'9' ||
         aChar >= L'A' && aChar <= L'F' ||
         aChar >= L'a' && aChar <= L'f';
}

inline bool
MatchUnicodeString(const UNICODE_STRING& aStr, bool (*aPredicate)(WCHAR))
{
  WCHAR* cur = aStr.Buffer;
  WCHAR* end = &aStr.Buffer[aStr.Length / sizeof(WCHAR)];
  while (cur < end) {
    if (!aPredicate(*cur)) {
      return false;
    }

    ++cur;
  }

  return true;
}

inline bool
Contains12DigitHexString(const UNICODE_STRING& aLeafName)
{
  uint16_t start, end;
  if (!FindCharInUnicodeString(aLeafName, L'.', start)) {
    return false;
  }

  ++start;
  if (!FindCharInUnicodeString(aLeafName, L'.', end, start)) {
    return false;
  }

  if (end - start != 12) {
    return false;
  }

  UNICODE_STRING test;
  test.Buffer = &aLeafName.Buffer[start];
  test.Length = (end - start) * sizeof(WCHAR);
  test.MaximumLength = test.Length;

  return MatchUnicodeString(test, &IsHexDigit);
}

inline bool
IsFileNameAtLeast16HexDigits(const UNICODE_STRING& aLeafName)
{
  uint16_t dotIndex;
  if (!FindCharInUnicodeString(aLeafName, L'.', dotIndex)) {
    return false;
  }

  if (dotIndex < 16) {
    return false;
  }

  UNICODE_STRING test;
  test.Buffer = aLeafName.Buffer;
  test.Length = dotIndex * sizeof(WCHAR);
  test.MaximumLength = aLeafName.MaximumLength;

  return MatchUnicodeString(test, &IsHexDigit);
}

inline void
GetLeafName(PUNICODE_STRING aDestString, PCUNICODE_STRING aSrcString)
{
  WCHAR* buf = aSrcString->Buffer;
  WCHAR* end = &aSrcString->Buffer[(aSrcString->Length / sizeof(WCHAR)) - 1];
  WCHAR* cur = end;
  while (cur >= buf) {
    if (*cur == L'\\') {
      break;
    }

    --cur;
  }

  // At this point, either cur points to the final backslash, or it points to
  // buf - 1. Either way, we're interested in cur + 1 as the desired buffer.
  aDestString->Buffer = cur + 1;
  aDestString->Length = (end - aDestString->Buffer + 1) * sizeof(WCHAR);
  aDestString->MaximumLength = aDestString->Length;
}

inline char
EnsureLowerCaseASCII(char aChar)
{
  if (aChar >= 'A' && aChar <= 'Z') {
    aChar -= 'A' - 'a';
  }

  return aChar;
}

inline int
StricmpASCII(const char* aLeft, const char* aRight)
{
  char curLeft, curRight;

  do {
    curLeft = EnsureLowerCaseASCII(*(aLeft++));
    curRight = EnsureLowerCaseASCII(*(aRight++));
  } while(curLeft && curLeft == curRight);

  return curLeft - curRight;
}

class MOZ_RAII PEHeaders final
{
  /**
   * This structure is documented on MSDN as VS_VERSIONINFO, but is not present
   * in SDK headers because it cannot be specified as a C struct. The following
   * structure contains the fixed-length fields at the beginning of
   * VS_VERSIONINFO.
   */
  struct VS_VERSIONINFO_HEADER
  {
    WORD             wLength;
    WORD             wValueLength;
    WORD             wType;
    WCHAR            szKey[16]; // ArrayLength(L"VS_VERSION_INFO")
    // Additional data goes here, aligned on a 4-byte boundary
  };

public:
  explicit PEHeaders(void* aBaseAddress)
    : PEHeaders(reinterpret_cast<PIMAGE_DOS_HEADER>(aBaseAddress))
  {
  }

  // The lowest two bits of an HMODULE are used as flags. Stripping those bits
  // from the HMODULE yields the base address of the binary's memory mapping.
  // (See LoadLibraryEx docs on MSDN)
  explicit PEHeaders(HMODULE aModule)
    : PEHeaders(reinterpret_cast<PIMAGE_DOS_HEADER>(
                  reinterpret_cast<uintptr_t>(aModule) & ~uintptr_t(3)))
  {
  }

  explicit operator bool() const
  {
    return !!mImageLimit;
  }

  /**
   * This overload computes absolute virtual addresses relative to the base
   * address of the binary.
   */
  template <typename T, typename R>
  T RVAToPtr(R aRva)
  {
    return RVAToPtr<T>(mMzHeader, aRva);
  }

  /**
   * This overload computes a result by adding aRva to aBase, but also ensures
   * that the resulting pointer falls within the bounds of this binary's memory
   * mapping.
   */
  template <typename T, typename R>
  T RVAToPtr(void* aBase, R aRva)
  {
    if (!mImageLimit) {
      return nullptr;
    }

    char* absAddress = reinterpret_cast<char*>(aBase) + aRva;
    if (absAddress < reinterpret_cast<char*>(mMzHeader) ||
        absAddress > reinterpret_cast<char*>(mImageLimit)) {
      return nullptr;
    }

    return reinterpret_cast<T>(absAddress);
  }

  PIMAGE_IMPORT_DESCRIPTOR GetImportDirectory()
  {
    return GetImageDirectoryEntry<PIMAGE_IMPORT_DESCRIPTOR>(
      IMAGE_DIRECTORY_ENTRY_IMPORT);
  }

  PIMAGE_RESOURCE_DIRECTORY GetResourceTable()
  {
    return GetImageDirectoryEntry<PIMAGE_RESOURCE_DIRECTORY>(
      IMAGE_DIRECTORY_ENTRY_RESOURCE);
  }

  bool
  GetVersionInfo(uint64_t& aOutVersion)
  {
    // RT_VERSION == 16
    // Version resources require an id of 1
    auto root = FindResourceLeaf<VS_VERSIONINFO_HEADER*>(16, 1);
    if (!root) {
      return false;
    }

    VS_FIXEDFILEINFO* fixedInfo = GetFixedFileInfo(root);
    if (!fixedInfo) {
      return false;
    }

    aOutVersion = ((static_cast<uint64_t>(fixedInfo->dwFileVersionMS) << 32) |
                   static_cast<uint64_t>(fixedInfo->dwFileVersionLS));
    return true;
  }

  bool
  GetTimeStamp(DWORD& aResult)
  {
    if (!(*this)) {
      return false;
    }

    aResult = mPeHeader->FileHeader.TimeDateStamp;
    return true;
  }

  PIMAGE_IMPORT_DESCRIPTOR
  GetIATForModule(const char* aModuleNameASCII)
  {
    for (PIMAGE_IMPORT_DESCRIPTOR curImpDesc = GetImportDirectory();
         IsValid(curImpDesc); ++curImpDesc) {
      auto curName = RVAToPtr<const char*>(curImpDesc->Name);
      if (!curName) {
        return nullptr;
      }

      if (StricmpASCII(aModuleNameASCII, curName)) {
        continue;
      }

      // curImpDesc now points to the IAT for the module we're interested in
      return curImpDesc;
    }

    return nullptr;
  }

  /**
   * Resources are stored in a three-level tree. To locate a particular entry,
   * you must supply a resource type, the resource id, and then the language id.
   * If aLangId == 0, we just resolve the first entry regardless of language.
   */
  template <typename T> T
  FindResourceLeaf(WORD aType, WORD aResId, WORD aLangId = 0)
  {
    PIMAGE_RESOURCE_DIRECTORY topLevel = GetResourceTable();
    if (!topLevel) {
      return nullptr;
    }

    PIMAGE_RESOURCE_DIRECTORY_ENTRY typeEntry = FindResourceEntry(topLevel,
                                                                  aType);
    if (!typeEntry || !typeEntry->DataIsDirectory) {
      return nullptr;
    }

    auto idDir = RVAToPtr<PIMAGE_RESOURCE_DIRECTORY>(topLevel,
                                                     typeEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY idEntry = FindResourceEntry(idDir, aResId);
    if (!idEntry || !idEntry->DataIsDirectory) {
      return nullptr;
    }

    auto langDir = RVAToPtr<PIMAGE_RESOURCE_DIRECTORY>(topLevel,
                                                       idEntry->OffsetToDirectory);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY langEntry;
    if (aLangId) {
      langEntry = FindResourceEntry(langDir, aLangId);
    } else {
      langEntry = FindFirstResourceEntry(langDir);
    }

    if (!langEntry || langEntry->DataIsDirectory) {
      return nullptr;
    }

    auto dataEntry = RVAToPtr<PIMAGE_RESOURCE_DATA_ENTRY>(topLevel,
                                                          langEntry->OffsetToData);
    return RVAToPtr<T>(dataEntry->OffsetToData);
  }

  static bool IsValid(PIMAGE_IMPORT_DESCRIPTOR aImpDesc)
  {
    return aImpDesc && aImpDesc->OriginalFirstThunk != 0;
  }

  static bool IsValid(PIMAGE_THUNK_DATA aImgThunk)
  {
    return aImgThunk && aImgThunk->u1.Ordinal != 0;
  }

private:
  explicit PEHeaders(PIMAGE_DOS_HEADER aMzHeader)
    : mMzHeader(aMzHeader)
    , mPeHeader(nullptr)
    , mImageLimit(nullptr)
  {
    if (!mMzHeader || mMzHeader->e_magic != IMAGE_DOS_SIGNATURE) {
      return;
    }

    mPeHeader = RVAToPtrUnchecked<PIMAGE_NT_HEADERS>(mMzHeader->e_lfanew);
    if (!mPeHeader || mPeHeader->Signature != IMAGE_NT_SIGNATURE) {
      return;
    }

    mImageLimit =
      RVAToPtrUnchecked<void*>(mPeHeader->OptionalHeader.SizeOfImage - 1UL);
  }

  template <typename T>
  T GetImageDirectoryEntry(unsigned int aDirectoryIndex)
  {
    if (aDirectoryIndex >= IMAGE_NUMBEROF_DIRECTORY_ENTRIES) {
      return nullptr;
    }

    IMAGE_DATA_DIRECTORY& dirEntry =
      mPeHeader->OptionalHeader.DataDirectory[aDirectoryIndex];
    return RVAToPtr<T>(dirEntry.VirtualAddress);
  }

  // This private overload does not have bounds checks, because we need to be
  // able to resolve the bounds themselves.
  template <typename T, typename R>
  T RVAToPtrUnchecked(R aRva)
  {
    return reinterpret_cast<T>(reinterpret_cast<char*>(mMzHeader) + aRva);
  }

  PIMAGE_RESOURCE_DIRECTORY_ENTRY
  FindResourceEntry(PIMAGE_RESOURCE_DIRECTORY aCurLevel, WORD aId)
  {
    // Immediately after the IMAGE_RESOURCE_DIRECTORY structure is an array
    // of IMAGE_RESOURCE_DIRECTORY_ENTRY structures. Since this function
    // searches by ID, we need to skip past any named entries before iterating.
    auto dirEnt =
      reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(aCurLevel + 1) +
      aCurLevel->NumberOfNamedEntries;
    for (WORD i = 0; i < aCurLevel->NumberOfIdEntries; ++i) {
      if (dirEnt[i].Id == aId) {
        return &dirEnt[i];
      }
    }

    return nullptr;
  }

  PIMAGE_RESOURCE_DIRECTORY_ENTRY
  FindFirstResourceEntry(PIMAGE_RESOURCE_DIRECTORY aCurLevel)
  {
    // Immediately after the IMAGE_RESOURCE_DIRECTORY structure is an array
    // of IMAGE_RESOURCE_DIRECTORY_ENTRY structures. We just return the first
    // entry, regardless of whether it is indexed by name or by id.
    auto dirEnt = reinterpret_cast<PIMAGE_RESOURCE_DIRECTORY_ENTRY>(aCurLevel + 1);
    WORD numEntries = aCurLevel->NumberOfNamedEntries +
                      aCurLevel->NumberOfIdEntries;
    if (!numEntries) {
      return nullptr;
    }

    return dirEnt;
  }

  VS_FIXEDFILEINFO*
  GetFixedFileInfo(VS_VERSIONINFO_HEADER* aVerInfo)
  {
    WORD length = aVerInfo->wLength;
    WORD offset = sizeof(VS_VERSIONINFO_HEADER);
    if (!offset) {
      return nullptr;
    }

    const wchar_t kVersionInfoKey[] = L"VS_VERSION_INFO";
    if (::RtlCompareMemory(aVerInfo->szKey, kVersionInfoKey,
                           ArrayLength(kVersionInfoKey)) !=
        ArrayLength(kVersionInfoKey)) {
      return nullptr;
    }

    uintptr_t base = reinterpret_cast<uintptr_t>(aVerInfo);
    // Align up to 4-byte boundary
#pragma warning(suppress: 4146)
    offset += (-(base + offset) & 3);

    if (offset > length) {
      return nullptr;
    }

    auto result = reinterpret_cast<VS_FIXEDFILEINFO*>(base + offset);
    if (result->dwSignature != 0xFEEF04BD) {
      return nullptr;
    }

    return result;
  }

private:
  PIMAGE_DOS_HEADER mMzHeader;
  PIMAGE_NT_HEADERS mPeHeader;
  void*             mImageLimit;
};

inline HANDLE
RtlGetProcessHeap()
{
  PTEB teb = ::NtCurrentTeb();
  PPEB peb = teb->ProcessEnvironmentBlock;
  return peb->Reserved4[1];
}

} // namespace nt
} // namespace mozilla

#endif // mozilla_NativeNt_h
